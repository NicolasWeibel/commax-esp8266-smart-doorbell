#pragma once

#include <Arduino.h>

#include "AppTypes.h"

namespace AppConfig {

// -----------------------------------------------------------------------------
// Firmware identity
// -----------------------------------------------------------------------------
constexpr char kFirmwareName[] = "COMMAX ESP8266 Smart Doorbell";
constexpr char kFirmwareVersion[] = "0.1.0-stage1";
constexpr char kImplementationStage[] =
    "Stage 1 - base structure and diagnostics";

// -----------------------------------------------------------------------------
// Serial console
// -----------------------------------------------------------------------------
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSerialStartupDelayMs = 250;

// -----------------------------------------------------------------------------
// Periodic diagnostics
// -----------------------------------------------------------------------------
constexpr uint32_t kHeartbeatIntervalMs = 10000;
constexpr uint32_t kDetailedMemoryIntervalMs = 60000;

// -----------------------------------------------------------------------------
// Wemos D1 mini built-in LED
// The built-in LED is normally active LOW.
// -----------------------------------------------------------------------------
constexpr uint8_t kStatusLedPin = LED_BUILTIN;
constexpr bool kStatusLedActiveLow = true;
constexpr uint32_t kStatusLedPulseMs = 50;

// -----------------------------------------------------------------------------
// Informational memory thresholds for Stage 1
// These thresholds only produce serial warnings; they do not restart the board.
// -----------------------------------------------------------------------------
constexpr uint32_t kHeapWarningBytes = 16000;
constexpr uint8_t kHeapFragmentationWarningPercent = 40;

// -----------------------------------------------------------------------------
// Compile-time options
// -----------------------------------------------------------------------------
constexpr bool kEnableHeartbeatLed = true;
constexpr LogLevel kCompileTimeLogLevel = LogLevel::Info;

}  // namespace AppConfig
