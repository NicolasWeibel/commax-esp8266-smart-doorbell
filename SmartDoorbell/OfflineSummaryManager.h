#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"

class OfflineSummaryManager {
 public:
  void begin(uint32_t bootNonce);
  void restore(const PersistentOfflineSummary& persisted,
               uint32_t currentBootNonce);

  void activate(const char* sequenceId);
  void addHistorical(uint32_t presses,
                     uint32_t occasions,
                     bool latestEpochValid,
                     time_t latestEpoch);
  void setLatestExact(time_t epoch);
  void setLatestRelative(uint64_t monotonicMs);
  void setLatestUnknown();
  void recordBell(uint64_t nowMs, bool epochValid, time_t epoch);
  void tick(uint64_t nowMs);
  void markInitialConfirmed();
  void clear();

  bool active() const;
  bool hasBells() const;
  OfflineSummaryStatus status() const;
  PersistentOfflineSummary persistentState() const;
  bool lastBellAgeSeconds(uint64_t nowMs,
                          time_t currentEpoch,
                          uint64_t& ageSeconds) const;

 private:
  OfflineSummaryStatus status_{};
  uint32_t bootNonce_ = 0;
};
