#pragma once

#include <Arduino.h>

class MonotonicClock {
 public:
  uint64_t nowMs();

 private:
  uint32_t lastRawMs_ = 0;
  uint64_t rolloverBaseMs_ = 0;
  bool initialized_ = false;
};
