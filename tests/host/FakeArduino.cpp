#include "Arduino.h"
#include "FakeArduino.h"

namespace {

uint64_t currentUs = 0;
bool d5High = false;
InterruptHandler interruptHandler = nullptr;

}  // namespace

uint32_t millis() {
  return static_cast<uint32_t>(currentUs / 1000ULL);
}

uint32_t micros() {
  return static_cast<uint32_t>(currentUs);
}

void pinMode(uint8_t, uint8_t) {}

int digitalRead(uint8_t pin) {
  return (pin == D5 && d5High) ? HIGH : LOW;
}

void digitalWrite(uint8_t, uint8_t) {}

int digitalPinToInterrupt(uint8_t pin) {
  return pin;
}

void attachInterrupt(int, InterruptHandler handler, int) {
  interruptHandler = handler;
}

void noInterrupts() {}
void interrupts() {}
void yield() {}

void delay(uint32_t milliseconds) {
  currentUs += static_cast<uint64_t>(milliseconds) * 1000ULL;
}

namespace FakeArduino {

void reset(bool initialD5High) {
  currentUs = 0;
  d5High = initialD5High;
  interruptHandler = nullptr;
}

void setD5(bool high, bool triggerInterrupt) {
  if (d5High == high) {
    return;
  }

  d5High = high;
  if (triggerInterrupt && interruptHandler != nullptr) {
    interruptHandler();
  }
}

void advanceMs(uint32_t milliseconds) {
  currentUs += static_cast<uint64_t>(milliseconds) * 1000ULL;
}

void advanceUs(uint32_t microseconds) {
  currentUs += microseconds;
}

void setTimeUs(uint64_t microseconds) {
  currentUs = microseconds;
}

uint64_t nowUs64() {
  return currentUs;
}

}  // namespace FakeArduino
