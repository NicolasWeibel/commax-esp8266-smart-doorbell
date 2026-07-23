#pragma once

#include <cstdint>

namespace FakeArduino {

void reset(bool initialD5High = false);
void setD5(bool high, bool triggerInterrupt = true);
void advanceMs(uint32_t milliseconds);
void advanceUs(uint32_t microseconds);
void setTimeUs(uint64_t microseconds);
uint64_t nowUs64();

}  // namespace FakeArduino
