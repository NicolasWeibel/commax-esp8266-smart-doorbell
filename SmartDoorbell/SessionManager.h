#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"

class SessionManager {
 public:
  void begin();

  BellSessionResult onValidBell(uint64_t nowMs,
                                bool epochValid,
                                time_t epoch,
                                const char* newSequenceId);
  void tick(uint64_t nowMs);

  bool popEvent(SessionEvent& event);
  SessionStatus status() const;

  bool restore(const PersistentSessionState& persisted,
               uint64_t nowMs,
               bool timeValid,
               time_t currentEpoch);
  PersistentSessionState persistentState() const;
  void reset();

 private:
  bool shouldStartNew(uint64_t nowMs) const;
  bool shouldStartInsistence(uint64_t nowMs) const;
  void startNew(uint64_t nowMs,
                bool epochValid,
                time_t epoch,
                const char* sequenceId);
  void updateLastPress(uint64_t nowMs, bool epochValid, time_t epoch);
  void emit(SessionEventType type,
            NotificationStage stage,
            const char* sequenceId);
  void closeAfterExpiration();

  SessionStatus status_{};
  SessionEvent events_[AppConfig::kSessionEventQueueCapacity]{};
  uint8_t eventHead_ = 0;
  uint8_t eventTail_ = 0;
};
