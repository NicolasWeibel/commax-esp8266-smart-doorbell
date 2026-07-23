#pragma once

#include <Arduino.h>

#include "AppTypes.h"

namespace AppConfig {

// -----------------------------------------------------------------------------
// Firmware identity
// -----------------------------------------------------------------------------
constexpr char kFirmwareName[] = "COMMAX ESP8266 Smart Doorbell";
constexpr char kFirmwareVersion[] = "0.2.0-stage2";
constexpr char kImplementationStage[] =
    "Stage 2 - D5 input, edge queue, and bell pulse detection";

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
constexpr uint32_t kHeartbeatLedPulseMs = 50;
constexpr uint32_t kValidBellLedPulseMs = 300;

// -----------------------------------------------------------------------------
// Optocoupler output input
// D5 maps to GPIO14 on the LOLIN(WEMOS) D1 R2 & mini profile.
// INPUT is intentional because the hardware uses an external 10 kOhm pull-up
// to 3V3. The expected signal is active HIGH:
//   LOW  = intercom idle, optocoupler conducting
//   HIGH = bell line active or intercom input unavailable
// -----------------------------------------------------------------------------
constexpr uint8_t kBellInputPin = D5;
constexpr uint8_t kBellInputMode = INPUT;
constexpr bool kRestingLevelHigh = false;
constexpr bool kActiveLevelHigh = true;
static_assert(kRestingLevelHigh != kActiveLevelHigh,
              "Bell resting and active levels must differ.");

// -----------------------------------------------------------------------------
// Electrical detection timing
// -----------------------------------------------------------------------------
constexpr uint32_t kBootWaitMs = 5000;
constexpr uint32_t kArmLowStableMs = 2000;
constexpr uint32_t kMinimumBellHighMs = 80;
constexpr uint32_t kMaximumBellHighMs = 15000;
// A new press is accepted after 100 ms of continuous LOW. A HIGH that starts
// earlier is treated as bounce/continuation, but it is still monitored for the
// 15-second abnormal-signal limit.
constexpr uint32_t kRearmLowStableMs = 100;

// -----------------------------------------------------------------------------
// Fixed-capacity queues; no dynamic allocation is used by the detector.
// The raw edge queue reserves one slot to distinguish full from empty, so a
// capacity of 64 stores up to 63 pending edges.
// -----------------------------------------------------------------------------
constexpr uint8_t kRawEdgeQueueCapacity = 64;
constexpr uint8_t kBellEventQueueCapacity = 16;

static_assert(kRawEdgeQueueCapacity >= 4,
              "The raw edge queue is too small.");
static_assert((kRawEdgeQueueCapacity & (kRawEdgeQueueCapacity - 1U)) == 0,
              "The raw edge queue capacity must be a power of two.");
static_assert(kRawEdgeQueueCapacity <= 128,
              "The raw edge queue uses 8-bit indices.");
static_assert(kBellEventQueueCapacity >= 4,
              "The bell event queue is too small.");

// -----------------------------------------------------------------------------
// Informational memory thresholds
// These thresholds only emit serial warnings; they never restart the board.
// -----------------------------------------------------------------------------
constexpr uint32_t kHeapWarningBytes = 16000;
constexpr uint8_t kHeapFragmentationWarningPercent = 40;

// -----------------------------------------------------------------------------
// Compile-time options
// -----------------------------------------------------------------------------
constexpr bool kEnableHeartbeatLed = true;
constexpr bool kEnableValidBellLed = true;
constexpr LogLevel kCompileTimeLogLevel = LogLevel::Info;

}  // namespace AppConfig
