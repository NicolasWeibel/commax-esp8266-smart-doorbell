#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"
#include "EventQueue.h"

class BellDetector {
 public:
  BellDetector() = default;

  void begin();
  void tick();

  bool popEvent(BellEvent& event);
  BellDetectorStatus status() const;
  bool activityInFlight() const;

 private:
  static void IRAM_ATTR interruptThunk();
  void IRAM_ATTR captureEdgeFromIsr();

  void processEdge(const EdgeEvent& edge,
                   uint32_t edgeOccurredAtMs,
                   bool timingReliable);
  void updateTimedTransitions(uint32_t nowMs, uint32_t nowUs);
  void startEligibleHighPulse(const EdgeEvent& edge,
                              uint32_t edgeOccurredAtMs,
                              bool timingReliable);
  void startHighDuringRearm(const EdgeEvent& edge,
                            bool timingReliable);
  void finishHighPulse(uint32_t durationUs, uint32_t edgeOccurredAtMs);
  void finishHighDuringRearm(uint32_t durationUs,
                             uint32_t edgeOccurredAtMs);
  void reportHighTooLong(uint32_t occurredAtMs, uint32_t durationUs);
  void discardUnreliableHigh(uint32_t occurredAtMs);

  void enterWaitingForStableLow(uint32_t nowMs);
  void enterWaitingForRearm(uint32_t nowMs);
  void startLowStableTracking(uint32_t nowMs);
  void stopLowStableTracking();
  void arm(uint32_t nowMs);

  void emitEvent(BellEventType type,
                 uint32_t occurredAtMs,
                 uint32_t pulseDurationUs = 0,
                 uint32_t value = 0);

  bool isRestingLevel() const;
  bool isActiveLevel() const;
  bool hasCompletedRearmAt(uint32_t occurredAtMs) const;

  static BellDetector* instance_;

  EdgeEventQueue rawEdgeQueue_{};

  BellDetectorState state_ = BellDetectorState::BootWait;
  BellDetectorStats stats_{};
  bool currentLevelHigh_ = false;

  uint32_t bootStartedMs_ = 0;
  uint32_t lowStableSinceMs_ = 0;
  bool lowStableTracking_ = false;
  uint32_t highStartedUs_ = 0;
  bool highTimingReliable_ = false;

  BellEvent events_[AppConfig::kBellEventQueueCapacity]{};
  uint8_t eventHead_ = 0;
  uint8_t eventTail_ = 0;
};
