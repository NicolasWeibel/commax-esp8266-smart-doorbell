#pragma once

#include <Arduino.h>

#include "AppTypes.h"

namespace AppConfig {

// Firmware identity
constexpr char kFirmwareName[] = "COMMAX ESP8266 Smart Doorbell";
constexpr char kFirmwareVersion[] = "1.0.0-rc17-production-text-only-natural-time";
constexpr char kImplementationStage[] =
    "Production candidate - quieter diagnostics, DHCP-only networking, and reliable text-only delivery";

// Serial console and diagnostics
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSerialStartupDelayMs = 250;
constexpr uint32_t kHeartbeatIntervalMs = 60000;
constexpr uint32_t kDetailedMemoryIntervalMs = 600000;
constexpr uint32_t kServiceStatusIntervalMs = 300000;

// Wemos D1 mini built-in LED (active LOW)
constexpr uint8_t kStatusLedPin = LED_BUILTIN;
constexpr bool kStatusLedActiveLow = true;
constexpr uint32_t kHeartbeatLedPulseMs = 50;
constexpr uint32_t kValidBellLedPulseMs = 300;

// Bell input: D5/GPIO14 with external 10 kOhm pull-up.
// LOW = idle, HIGH = bell active or optocoupler input unavailable.
constexpr uint8_t kBellInputPin = D5;
constexpr uint8_t kBellInputMode = INPUT;
constexpr bool kRestingLevelHigh = false;
constexpr bool kActiveLevelHigh = true;
constexpr uint32_t kBootWaitMs = 5000;
constexpr uint32_t kArmLowStableMs = 2000;
constexpr uint32_t kMinimumBellHighMs = 80;
constexpr uint32_t kMaximumBellHighMs = 15000;
constexpr uint32_t kRearmLowStableMs = 100;
constexpr uint8_t kRawEdgeQueueCapacity = 64;
// A single raw-queue drain can emit several diagnostic events per pulse.
// Keep room for the whole batch so diagnostics cannot crowd out ValidPress.
constexpr uint8_t kBellEventQueueCapacity = 128;

// Session behavior
constexpr uint32_t kFirstWindowMs = 60000;
constexpr uint32_t kSessionSilenceTimeoutMs = 120000;
constexpr uint32_t kNotificationUpdateIntervalMs = 10000;
constexpr uint8_t kSessionEventQueueCapacity = 8;

// Retry series. These offsets are measured from the beginning of the current
// failure series, so the important attempts remain inside the first minute.
constexpr uint32_t kRetryOffset1Ms = 3000;
constexpr uint32_t kRetryOffset2Ms = 10000;
constexpr uint32_t kRetryOffset3Ms = 25000;
constexpr uint32_t kRetryOffset4Ms = 45000;
constexpr uint32_t kRetryAfterSeriesMs = 30000;
constexpr uint32_t kRetryOfflineIntervalMs = 60000;
constexpr uint32_t kMinimumRetrySpacingMs = 2000;
constexpr uint32_t kOfflineFailureWindowMs = 60000;
constexpr uint32_t kOfflineRecoveryProbeIntervalMs = 60000;
constexpr uint32_t kRecoveryInputSettlePollMs = 100;
constexpr uint32_t kOfflineOccasionSilenceMs = kSessionSilenceTimeoutMs;

// Connectivity policy
constexpr uint8_t kDegradedAfterFailures = 2;
constexpr uint8_t kOfflineAfterFailures = 4;
constexpr uint32_t kWifiOfflineConfirmationMs = 5000;
constexpr uint32_t kWifiReconnectIntervalMs = 15000;

// Production Wi-Fi health monitoring. Detailed event history remains available
// with the serial commands, while automatic recovery uses a bounded backoff.
constexpr uint32_t kWifiHealthPollIntervalMs = 500;
constexpr uint32_t kWifiInvalidIpv4ConfirmationMs = 500;
constexpr uint32_t kWifiFaultHistoryWindowMs = 30000;
constexpr uint32_t kWifiOutageSummaryIntervalMs = 60000;
constexpr uint32_t kWifiRecoveryBackoffAfter1Ms = 10000;
constexpr uint32_t kWifiRecoveryBackoffAfter2Ms = 30000;
constexpr uint32_t kWifiRecoveryBackoffAfter3Ms = 60000;
constexpr uint32_t kWifiRecoverySeriesResetMs = 300000;

// Wi-Fi and timekeeping
constexpr uint32_t kNtpPollIntervalMs = 1000;
constexpr time_t kMinimumValidEpoch = 1704067200;  // 2024-01-01 UTC

// TLS and ntfy text transport. Keep-alive is retained for fast counter updates.
constexpr uint16_t kNtfyHttpTimeoutMs = 5500;
constexpr uint32_t kNtfyKeepAliveIdleMs = 30000;
constexpr uint32_t kNtfyFreshReconnectDelayMs = 150;
constexpr uint8_t kWifiRecycleAfterConnectFailures = 255;
constexpr uint32_t kWifiForcedRecycleCooldownMs = 120000;
constexpr uint16_t kNtfyTlsReceiveBufferBytes = 512;
constexpr uint16_t kNtfyTlsTransmitBufferBytes = 512;

// Layered diagnostics remain available on demand. They are not executed
// automatically in the production candidate.
constexpr uint32_t kNetworkDiagnosticsAutomaticCooldownMs = 60000;
constexpr uint32_t kNetworkDiagnosticsConnectTimeoutMs = 3000;
constexpr uint32_t kNetworkDiagnosticsInterProbeDelayMs = 120;
constexpr char kNetworkDiagnosticsAlternateHost[] = "example.com";
constexpr uint16_t kNetworkDiagnosticsAlternatePort = 443;
constexpr char kCertIndexPath[] = "/certs.idx";
constexpr char kCertArchivePath[] = "/certs.ar";

// LittleFS state paths. Versioned paths prevent an RC7 record from being
// interpreted as the current persistent v3 layout.
constexpr char kStatePath[] = "/doorbell-v4.bin";
constexpr char kStateBackupPath[] = "/doorbell-v4.bak";
constexpr char kStateTemporaryPath[] = "/doorbell-v4.tmp";
constexpr char kLegacyStatePath[] = "/doorbell-v3.bin";
constexpr char kLegacyStateBackupPath[] = "/doorbell-v3.bak";
constexpr char kLegacyStateTemporaryPath[] = "/doorbell-v3.tmp";

// Informational memory thresholds
constexpr uint32_t kHeapWarningBytes = 15000;
constexpr uint8_t kHeapFragmentationWarningPercent = 40;

// Compile-time production options
constexpr bool kEnableHeartbeatLed = true;
constexpr bool kEnableValidBellLed = true;
constexpr bool kPrintDetailedWifiEventsLive = false;
constexpr bool kVerboseOperationalLogs = false;
constexpr bool kEnableAutomaticLayeredDiagnostics = false;
constexpr LogLevel kCompileTimeLogLevel = LogLevel::Info;

static_assert(kRestingLevelHigh != kActiveLevelHigh,
              "Bell resting and active levels must differ.");
static_assert((kRawEdgeQueueCapacity & (kRawEdgeQueueCapacity - 1U)) == 0,
              "The raw edge queue capacity must be a power of two.");
static_assert(kRawEdgeQueueCapacity <= 128,
              "The raw edge queue uses 8-bit indices.");
static_assert(kBellEventQueueCapacity >= 4,
              "The bell event queue is too small.");
static_assert(kBellEventQueueCapacity >= 2U * kRawEdgeQueueCapacity,
              "The bell event queue must accommodate a raw edge batch.");
static_assert(kSessionEventQueueCapacity >= 4,
              "The session event queue is too small.");
static_assert(kOfflineAfterFailures > kDegradedAfterFailures,
              "Offline must require more failures than degraded mode.");

}  // namespace AppConfig
