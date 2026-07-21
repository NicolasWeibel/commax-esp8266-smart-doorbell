#pragma once

#include <Arduino.h>

#include "AppTypes.h"

class Diagnostics {
 public:
  Diagnostics() = default;

  void begin();
  void tick();

  RuntimeStats stats() const;
  AppLifecycleState lifecycleState() const;

  void log(LogLevel level, const __FlashStringHelper* message) const;
  void log(LogLevel level, const char* message) const;

 private:
  void configureStatusLed();
  void setStatusLed(bool enabled);
  void startStatusLedPulse(uint32_t nowMs);
  void updateStatusLed(uint32_t nowMs);

  void sampleRuntimeStats(uint32_t nowMs);
  void printBootReport();
  void printHeartbeat(uint32_t nowMs);
  void printDetailedMemoryReport();
  void checkMemoryWarnings();

  bool shouldLog(LogLevel level) const;
  const __FlashStringHelper* levelText(LogLevel level) const;

  AppLifecycleState lifecycleState_ = AppLifecycleState::Starting;
  RuntimeStats stats_{};

  uint32_t lastHeartbeatMs_ = 0;
  uint32_t lastDetailedMemoryMs_ = 0;
  uint32_t ledPulseStartedMs_ = 0;
  bool ledPulseActive_ = false;
};
