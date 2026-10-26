#pragma once

#include <Arduino.h>
#include <time.h>

class TimeKeeper {
 public:
  void begin(uint64_t nowMs);
  void tick(uint64_t nowMs, bool wifiConnected);

  bool valid() const;
  time_t nowEpoch(uint64_t nowMs) const;
  bool reconstructEpoch(uint64_t eventMonotonicMs,
                        uint64_t nowMonotonicMs,
                        time_t& epochOut) const;
  bool formatLocal(time_t epoch, char* output, size_t outputSize) const;
  bool formatLocalTime(time_t epoch, char* output, size_t outputSize) const;
  bool consumeNewSynchronization();

 private:
  bool valid_ = false;
  bool configured_ = false;
  bool newSynchronization_ = false;
  time_t epochAnchor_ = 0;
  uint64_t monotonicAnchorMs_ = 0;
  uint64_t lastPollMs_ = 0;
};
