#include "SessionManager.h"

#include <cstring>

void SessionManager::begin() {
  reset();
}

BellSessionResult SessionManager::onValidBell(uint64_t nowMs,
                                              bool epochValid,
                                              time_t epoch,
                                              const char* newSequenceId) {
  if (shouldStartNew(nowMs)) {
    startNew(nowMs, epochValid, epoch, newSequenceId);
    return BellSessionResult::StartedFirst;
  }

  ++status_.totalPresses;
  updateLastPress(nowMs, epochValid, epoch);

  if (shouldStartInsistence(nowMs)) {
    status_.phase = SessionPhase::Insistence;
    if (newSequenceId != nullptr) {
      std::strncpy(status_.insistenceSequenceId,
                   newSequenceId,
                   sizeof(status_.insistenceSequenceId) - 1U);
    }
    return BellSessionResult::StartedInsistence;
  }

  return status_.phase == SessionPhase::Insistence
             ? BellSessionResult::UpdatedInsistence
             : BellSessionResult::UpdatedFirst;
}

void SessionManager::tick(uint64_t nowMs) {
  if (!status_.active) {
    return;
  }

  if (status_.phase == SessionPhase::FirstMinute &&
      nowMs >= status_.firstWindowEndsMs) {
    status_.phase = SessionPhase::FirstFinalPending;
    emit(SessionEventType::FirstWindowClosed,
         NotificationStage::FirstMinute,
         status_.firstSequenceId);
  }

  if (nowMs >= status_.sessionEndsMs) {
    const NotificationStage stage =
        status_.phase == SessionPhase::Insistence
            ? NotificationStage::Insistence
            : NotificationStage::FirstMinute;
    const char* sequenceId =
        stage == NotificationStage::Insistence
            ? status_.insistenceSequenceId
            : status_.firstSequenceId;
    emit(SessionEventType::SessionExpired, stage, sequenceId);
    closeAfterExpiration();
  }
}

bool SessionManager::popEvent(SessionEvent& event) {
  if (eventTail_ == eventHead_) {
    return false;
  }

  event = events_[eventTail_];
  eventTail_ = static_cast<uint8_t>(
      (eventTail_ + 1U) % AppConfig::kSessionEventQueueCapacity);
  return true;
}

SessionStatus SessionManager::status() const {
  return status_;
}

bool SessionManager::restore(const PersistentSessionState& persisted,
                             uint64_t nowMs,
                             bool timeValid,
                             time_t currentEpoch) {
  reset();
  if (persisted.active == 0U || !timeValid || currentEpoch <= 0 ||
      persisted.lastPressEpochValid == 0U || persisted.lastPressEpoch <= 0) {
    return false;
  }

  const time_t lastEpoch = static_cast<time_t>(persisted.lastPressEpoch);
  if (currentEpoch < lastEpoch) {
    return false;
  }

  const uint64_t lastAgeMs =
      static_cast<uint64_t>(currentEpoch - lastEpoch) * 1000ULL;
  if (lastAgeMs >= AppConfig::kSessionSilenceTimeoutMs) {
    return false;
  }

  status_.active = true;
  status_.totalPresses = persisted.totalPresses;
  status_.lastPressEpochValid = true;
  status_.lastPressEpoch = lastEpoch;
  // Events from the previous boot can predate this boot's monotonic origin.
  // Clamp their display timestamps, but preserve the exact remaining timers.
  status_.lastPressMs = nowMs >= lastAgeMs ? nowMs - lastAgeMs : 0;
  status_.sessionEndsMs =
      nowMs + (AppConfig::kSessionSilenceTimeoutMs - lastAgeMs);

  uint64_t firstAgeMs = lastAgeMs;
  if (persisted.firstPressEpochValid != 0U &&
      persisted.firstPressEpoch > 0 &&
      currentEpoch >= static_cast<time_t>(persisted.firstPressEpoch)) {
    const time_t firstEpoch = static_cast<time_t>(persisted.firstPressEpoch);
    firstAgeMs =
        static_cast<uint64_t>(currentEpoch - firstEpoch) * 1000ULL;
    status_.firstPressEpochValid = true;
    status_.firstPressEpoch = firstEpoch;
    status_.firstPressMs = nowMs >= firstAgeMs ? nowMs - firstAgeMs : 0;
  } else {
    status_.firstPressMs = status_.lastPressMs;
  }

  status_.firstWindowEndsMs = firstAgeMs >= AppConfig::kFirstWindowMs
                                  ? nowMs
                                  : nowMs + (AppConfig::kFirstWindowMs - firstAgeMs);
  std::strncpy(status_.firstSequenceId,
               persisted.firstSequenceId,
               sizeof(status_.firstSequenceId) - 1U);
  std::strncpy(status_.insistenceSequenceId,
               persisted.insistenceSequenceId,
               sizeof(status_.insistenceSequenceId) - 1U);

  SessionPhase savedPhase = static_cast<SessionPhase>(persisted.phase);
  if (savedPhase == SessionPhase::Idle) {
    savedPhase = nowMs >= status_.firstWindowEndsMs
                     ? SessionPhase::FirstFinalPending
                     : SessionPhase::FirstMinute;
  }
  if (savedPhase == SessionPhase::FirstMinute &&
      nowMs >= status_.firstWindowEndsMs) {
    savedPhase = SessionPhase::FirstFinalPending;
  }
  status_.phase = savedPhase;
  return true;
}

PersistentSessionState SessionManager::persistentState() const {
  PersistentSessionState persisted{};
  persisted.active = status_.active ? 1U : 0U;
  persisted.phase = static_cast<uint8_t>(status_.phase);
  persisted.firstPressEpochValid = status_.firstPressEpochValid ? 1U : 0U;
  persisted.lastPressEpochValid = status_.lastPressEpochValid ? 1U : 0U;
  persisted.totalPresses = status_.totalPresses;
  persisted.firstPressEpoch = static_cast<int64_t>(status_.firstPressEpoch);
  persisted.lastPressEpoch = static_cast<int64_t>(status_.lastPressEpoch);
  std::strncpy(persisted.firstSequenceId,
               status_.firstSequenceId,
               sizeof(persisted.firstSequenceId) - 1U);
  std::strncpy(persisted.insistenceSequenceId,
               status_.insistenceSequenceId,
               sizeof(persisted.insistenceSequenceId) - 1U);
  return persisted;
}

void SessionManager::reset() {
  status_ = SessionStatus{};
  eventHead_ = 0;
  eventTail_ = 0;
}

bool SessionManager::shouldStartNew(uint64_t nowMs) const {
  return !status_.active ||
         nowMs >= status_.sessionEndsMs ||
         (nowMs - status_.lastPressMs) >=
             AppConfig::kSessionSilenceTimeoutMs;
}

bool SessionManager::shouldStartInsistence(uint64_t nowMs) const {
  return status_.phase != SessionPhase::Insistence &&
         nowMs >= status_.firstWindowEndsMs;
}

void SessionManager::startNew(uint64_t nowMs,
                              bool epochValid,
                              time_t epoch,
                              const char* sequenceId) {
  status_ = SessionStatus{};
  status_.active = true;
  status_.phase = SessionPhase::FirstMinute;
  status_.totalPresses = 1;
  status_.firstPressMs = nowMs;
  status_.lastPressMs = nowMs;
  status_.firstWindowEndsMs = nowMs + AppConfig::kFirstWindowMs;
  status_.sessionEndsMs = nowMs + AppConfig::kSessionSilenceTimeoutMs;
  if (epochValid && epoch > 0) {
    status_.firstPressEpochValid = true;
    status_.lastPressEpochValid = true;
    status_.firstPressEpoch = epoch;
    status_.lastPressEpoch = epoch;
  }
  if (sequenceId != nullptr) {
    std::strncpy(status_.firstSequenceId,
                 sequenceId,
                 sizeof(status_.firstSequenceId) - 1U);
  }
}

void SessionManager::updateLastPress(uint64_t nowMs,
                                     bool epochValid,
                                     time_t epoch) {
  status_.lastPressMs = nowMs;
  status_.sessionEndsMs = nowMs + AppConfig::kSessionSilenceTimeoutMs;
  if (epochValid && epoch > 0) {
    status_.lastPressEpochValid = true;
    status_.lastPressEpoch = epoch;
  } else {
    status_.lastPressEpochValid = false;
    status_.lastPressEpoch = 0;
  }
}

void SessionManager::emit(SessionEventType type,
                          NotificationStage stage,
                          const char* sequenceId) {
  const uint8_t next = static_cast<uint8_t>(
      (eventHead_ + 1U) % AppConfig::kSessionEventQueueCapacity);
  if (next == eventTail_) {
    return;
  }

  SessionEvent& event = events_[eventHead_];
  event = SessionEvent{};
  event.type = type;
  event.stage = stage;
  event.totalPresses = status_.totalPresses;
  event.lastPressEpochValid = status_.lastPressEpochValid;
  event.lastPressEpoch = status_.lastPressEpoch;
  if (sequenceId != nullptr) {
    std::strncpy(event.sequenceId,
                 sequenceId,
                 sizeof(event.sequenceId) - 1U);
  }
  eventHead_ = next;
}

void SessionManager::closeAfterExpiration() {
  status_.active = false;
  status_.phase = SessionPhase::Idle;
}
