#include "MonotonicClock.h"

uint64_t MonotonicClock::nowMs() {
  const uint32_t raw = millis();

  if (!initialized_) {
    initialized_ = true;
    lastRawMs_ = raw;
    return raw;
  }

  if (raw < lastRawMs_) {
    rolloverBaseMs_ += (1ULL << 32U);
  }

  lastRawMs_ = raw;
  return rolloverBaseMs_ + raw;
}
