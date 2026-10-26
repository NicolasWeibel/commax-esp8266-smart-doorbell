#include "BellDetector.h"

#include "AppConfig.h"

BellDetector* BellDetector::instance_ = nullptr;

void BellDetector::begin() {
  pinMode(AppConfig::kBellInputPin, AppConfig::kBellInputMode);

  currentLevelHigh_ = digitalRead(AppConfig::kBellInputPin) == HIGH;
  state_ = BellDetectorState::BootWait;
  bootStartedMs_ = millis();
  stopLowStableTracking();
  highStartedUs_ = 0;
  highTimingReliable_ = false;

  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(AppConfig::kBellInputPin),
                  BellDetector::interruptThunk,
                  CHANGE);
}

void BellDetector::tick() {
  const uint32_t nowMs = millis();

  const uint32_t dropped = rawEdgeQueue_.takeDroppedCount();
  const bool edgeTimingReliable = dropped == 0U;
  if (dropped > 0U) {
    stats_.rawEdgesDropped += dropped;
    emitEvent(BellEventType::RawQueueOverflow, nowMs, 0, dropped);
    if (state_ == BellDetectorState::HighInProgress ||
        state_ == BellDetectorState::HighDuringRearm ||
        state_ == BellDetectorState::HighTooLong) {
      highTimingReliable_ = false;
    }
  }

  EdgeEvent edge{};
  while (rawEdgeQueue_.pop(edge)) {
    ++stats_.rawEdges;

    // Preserve the event time even if a future network operation delays the
    // main loop. Unsigned subtraction remains valid across micros() rollover as
    // long as an edge is not left queued for more than one micros() cycle.
    const uint32_t processedAtUs = micros();
    const uint32_t ageUs = processedAtUs - edge.timestampUs;
    const uint32_t edgeOccurredAtMs = nowMs - (ageUs / 1000U);
    processEdge(edge, edgeOccurredAtMs, edgeTimingReliable);
  }

  // Safety net: if an edge was lost, restore the logical state from the actual
  // D5 level. This does not replace the overflow counter; it prevents the state
  // machine from remaining permanently out of sync.
  const bool physicalLevelHigh =
      digitalRead(AppConfig::kBellInputPin) == HIGH;

  if (physicalLevelHigh != currentLevelHigh_) {
    ++stats_.inputResynchronizations;
    emitEvent(BellEventType::InputResynchronized,
              nowMs,
              0,
              physicalLevelHigh ? 1U : 0U);

    EdgeEvent syntheticEdge{};
    syntheticEdge.timestampUs = micros();
    syntheticEdge.levelHigh = physicalLevelHigh;
    processEdge(syntheticEdge, nowMs, false);
  }

  updateTimedTransitions(nowMs, micros());
}

bool BellDetector::popEvent(BellEvent& event) {
  if (eventTail_ == eventHead_) {
    return false;
  }

  event = events_[eventTail_];
  eventTail_ = static_cast<uint8_t>(
      (eventTail_ + 1U) % AppConfig::kBellEventQueueCapacity);
  return true;
}

BellDetectorStatus BellDetector::status() const {
  BellDetectorStatus result{};
  result.state = state_;
  result.currentLevelHigh = currentLevelHigh_;
  result.pendingRawEdges = rawEdgeQueue_.size();
  result.stats = stats_;
  return result;
}

bool BellDetector::activityInFlight() const {
  if (rawEdgeQueue_.size() > 0U || currentLevelHigh_) {
    return true;
  }
  return state_ == BellDetectorState::HighInProgress ||
         state_ == BellDetectorState::HighDuringRearm ||
         state_ == BellDetectorState::HighTooLong;
}

void IRAM_ATTR BellDetector::interruptThunk() {
  if (instance_ != nullptr) {
    instance_->captureEdgeFromIsr();
  }
}

void IRAM_ATTR BellDetector::captureEdgeFromIsr() {
  const bool levelHigh = digitalRead(AppConfig::kBellInputPin) == HIGH;
  rawEdgeQueue_.pushFromIsr(micros(), levelHigh);
}

void BellDetector::processEdge(const EdgeEvent& edge,
                               uint32_t edgeOccurredAtMs,
                               bool timingReliable) {
  // CHANGE should not normally deliver duplicate levels, but ignoring them
  // makes the detector robust to repeated or synthetic synchronization events.
  if (edge.levelHigh == currentLevelHigh_) {
    return;
  }

  currentLevelHigh_ = edge.levelHigh;

  switch (state_) {
    case BellDetectorState::BootWait:
      // Track only the current input level during the startup grace period.
      break;

    case BellDetectorState::WaitingForStableLow:
      if (isRestingLevel()) {
        startLowStableTracking(edgeOccurredAtMs);
      } else {
        stopLowStableTracking();
      }
      break;

    case BellDetectorState::Armed:
      if (isActiveLevel()) {
        startEligibleHighPulse(edge, edgeOccurredAtMs, timingReliable);
      }
      break;

    case BellDetectorState::HighInProgress:
      if (isRestingLevel()) {
        if (!highTimingReliable_) {
          discardUnreliableHigh(edgeOccurredAtMs);
        } else {
          const uint32_t durationUs = edge.timestampUs - highStartedUs_;
          finishHighPulse(durationUs, edgeOccurredAtMs);
        }
      }
      break;

    case BellDetectorState::HighDuringRearm:
      if (isRestingLevel()) {
        if (!highTimingReliable_) {
          discardUnreliableHigh(edgeOccurredAtMs);
        } else {
          const uint32_t durationUs = edge.timestampUs - highStartedUs_;
          finishHighDuringRearm(durationUs, edgeOccurredAtMs);
        }
      }
      break;

    case BellDetectorState::HighTooLong:
      if (isRestingLevel()) {
        if (!highTimingReliable_) {
          discardUnreliableHigh(edgeOccurredAtMs);
        } else {
          const uint32_t durationUs = edge.timestampUs - highStartedUs_;
          emitEvent(BellEventType::RecoveredFromHigh,
                    edgeOccurredAtMs,
                    durationUs,
                    stats_.highTooLongEvents);
          enterWaitingForRearm(edgeOccurredAtMs);
        }
      }
      break;

    case BellDetectorState::WaitingForRearm:
      if (isRestingLevel()) {
        startLowStableTracking(edgeOccurredAtMs);
      } else if (hasCompletedRearmAt(edgeOccurredAtMs)) {
        // The main loop may not have run at the exact 100 ms boundary. Use the
        // edge occurrence timestamp so a valid fast second press is not lost.
        arm(edgeOccurredAtMs);
        startEligibleHighPulse(edge, edgeOccurredAtMs, timingReliable);
      } else {
        // LOW did not remain stable long enough to distinguish a new press from
        // contact bounce or continuation of the previous electrical event.
        startHighDuringRearm(edge, timingReliable);
      }
      break;
  }
}

void BellDetector::updateTimedTransitions(uint32_t nowMs, uint32_t nowUs) {
  switch (state_) {
    case BellDetectorState::BootWait:
      if (static_cast<uint32_t>(nowMs - bootStartedMs_) >=
          AppConfig::kBootWaitMs) {
        emitEvent(BellEventType::BootWaitFinished,
                  nowMs,
                  0,
                  currentLevelHigh_ ? 1U : 0U);
        enterWaitingForStableLow(nowMs);
      }
      break;

    case BellDetectorState::WaitingForStableLow:
      if (isRestingLevel() && lowStableTracking_ &&
          static_cast<uint32_t>(nowMs - lowStableSinceMs_) >=
              AppConfig::kArmLowStableMs) {
        arm(nowMs);
      }
      break;

    case BellDetectorState::HighInProgress:
    case BellDetectorState::HighDuringRearm:
      if (isActiveLevel() && highTimingReliable_ &&
          (nowUs - highStartedUs_) >
              (AppConfig::kMaximumBellHighMs * 1000UL)) {
        reportHighTooLong(nowMs, nowUs - highStartedUs_);
      }
      break;

    case BellDetectorState::WaitingForRearm:
      if (isRestingLevel() && lowStableTracking_ &&
          static_cast<uint32_t>(nowMs - lowStableSinceMs_) >=
              AppConfig::kRearmLowStableMs) {
        arm(nowMs);
      }
      break;

    case BellDetectorState::Armed:
    case BellDetectorState::HighTooLong:
      break;
  }
}

void BellDetector::startEligibleHighPulse(const EdgeEvent& edge,
                                          uint32_t edgeOccurredAtMs,
                                          bool timingReliable) {
  highStartedUs_ = edge.timestampUs;
  highTimingReliable_ = timingReliable;
  state_ = BellDetectorState::HighInProgress;
  stopLowStableTracking();
  emitEvent(BellEventType::PulseStarted, edgeOccurredAtMs);
}

void BellDetector::startHighDuringRearm(const EdgeEvent& edge,
                                           bool timingReliable) {
  highStartedUs_ = edge.timestampUs;
  highTimingReliable_ = timingReliable;
  state_ = BellDetectorState::HighDuringRearm;
  stopLowStableTracking();
}

void BellDetector::finishHighPulse(uint32_t durationUs,
                                   uint32_t edgeOccurredAtMs) {
  if (durationUs < (AppConfig::kMinimumBellHighMs * 1000UL)) {
    ++stats_.shortPulsesIgnored;
    emitEvent(BellEventType::ShortPulseIgnored,
              edgeOccurredAtMs,
              durationUs,
              stats_.shortPulsesIgnored);
    enterWaitingForRearm(edgeOccurredAtMs);
    return;
  }

  if (durationUs <= (AppConfig::kMaximumBellHighMs * 1000UL)) {
    ++stats_.validPresses;
    emitEvent(BellEventType::ValidPress,
              edgeOccurredAtMs,
              durationUs,
              stats_.validPresses);
    enterWaitingForRearm(edgeOccurredAtMs);
    return;
  }

  // This path is possible if the main loop was delayed for more than 15 seconds
  // and later receives both the rising and falling edges together.
  reportHighTooLong(edgeOccurredAtMs, durationUs);
  emitEvent(BellEventType::RecoveredFromHigh,
            edgeOccurredAtMs,
            durationUs,
            stats_.highTooLongEvents);
  enterWaitingForRearm(edgeOccurredAtMs);
}

void BellDetector::finishHighDuringRearm(uint32_t durationUs,
                                         uint32_t edgeOccurredAtMs) {
  // A HIGH that begins before the stable-LOW rearm interval is complete is not
  // a new bell press. It is still monitored so a disconnected input or other
  // abnormal long HIGH cannot remain hidden in the rearm state.
  if (durationUs > (AppConfig::kMaximumBellHighMs * 1000UL)) {
    reportHighTooLong(edgeOccurredAtMs, durationUs);
    emitEvent(BellEventType::RecoveredFromHigh,
              edgeOccurredAtMs,
              durationUs,
              stats_.highTooLongEvents);
  }

  enterWaitingForRearm(edgeOccurredAtMs);
}

void BellDetector::reportHighTooLong(uint32_t occurredAtMs,
                                     uint32_t durationUs) {
  ++stats_.highTooLongEvents;
  state_ = BellDetectorState::HighTooLong;
  emitEvent(BellEventType::HighTooLong,
            occurredAtMs,
            durationUs,
            stats_.highTooLongEvents);
}

void BellDetector::discardUnreliableHigh(uint32_t occurredAtMs) {
  // A queue overflow or synthetic resynchronization means the exact rising
  // timestamp is no longer trustworthy. Never turn unsigned underflow into a
  // false multi-day HIGH_TOO_LONG report and never count that pulse as valid.
  highStartedUs_ = 0;
  highTimingReliable_ = false;
  enterWaitingForRearm(occurredAtMs);
}

void BellDetector::enterWaitingForStableLow(uint32_t nowMs) {
  state_ = BellDetectorState::WaitingForStableLow;
  highTimingReliable_ = false;
  highStartedUs_ = 0;

  if (isRestingLevel()) {
    startLowStableTracking(nowMs);
  } else {
    stopLowStableTracking();
  }
}

void BellDetector::enterWaitingForRearm(uint32_t nowMs) {
  state_ = BellDetectorState::WaitingForRearm;
  highTimingReliable_ = false;
  highStartedUs_ = 0;

  if (isRestingLevel()) {
    startLowStableTracking(nowMs);
  } else {
    stopLowStableTracking();
  }
}

void BellDetector::startLowStableTracking(uint32_t nowMs) {
  lowStableSinceMs_ = nowMs;
  lowStableTracking_ = true;
}

void BellDetector::stopLowStableTracking() {
  lowStableSinceMs_ = 0;
  lowStableTracking_ = false;
}

void BellDetector::arm(uint32_t nowMs) {
  state_ = BellDetectorState::Armed;
  highTimingReliable_ = false;
  highStartedUs_ = 0;
  stopLowStableTracking();
  emitEvent(BellEventType::Armed, nowMs, 0, stats_.validPresses);
}

void BellDetector::emitEvent(BellEventType type,
                             uint32_t occurredAtMs,
                             uint32_t pulseDurationUs,
                             uint32_t value) {
  const uint8_t next = static_cast<uint8_t>(
      (eventHead_ + 1U) % AppConfig::kBellEventQueueCapacity);

  if (next == eventTail_) {
    ++stats_.detectorEventsDropped;
    return;
  }

  BellEvent& event = events_[eventHead_];
  event.type = type;
  event.occurredAtMs = occurredAtMs;
  event.pulseDurationUs = pulseDurationUs;
  event.value = value;
  eventHead_ = next;
}

bool BellDetector::isRestingLevel() const {
  return currentLevelHigh_ == AppConfig::kRestingLevelHigh;
}

bool BellDetector::isActiveLevel() const {
  return currentLevelHigh_ == AppConfig::kActiveLevelHigh;
}

bool BellDetector::hasCompletedRearmAt(uint32_t occurredAtMs) const {
  return lowStableTracking_ &&
         static_cast<uint32_t>(occurredAtMs - lowStableSinceMs_) >=
             AppConfig::kRearmLowStableMs;
}
