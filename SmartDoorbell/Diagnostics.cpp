#include "Diagnostics.h"

#include <Esp.h>

#include "AppConfig.h"

namespace {

uint32_t bytesToKiB(uint32_t bytes) {
  return bytes / 1024U;
}

}  // namespace

void Diagnostics::begin() {
  configureStatusLed();

  Serial.begin(AppConfig::kSerialBaud);
  Serial.setDebugOutput(false);
  delay(AppConfig::kSerialStartupDelayMs);

  stats_.minimumFreeHeapBytes = ESP.getFreeHeap();
  sampleRuntimeStats(millis());
  printBootReport();

  lifecycleState_ = AppLifecycleState::Running;
  log(LogLevel::Info, F("Diagnostics initialized."));

  lastHeartbeatMs_ = millis();
  lastDetailedMemoryMs_ = millis();
  startStatusLedPulse(millis(), AppConfig::kHeartbeatLedPulseMs);
}

void Diagnostics::tick(const BellDetectorStatus& bellStatus) {
  const uint32_t nowMs = millis();

  ++stats_.loopCount;
  sampleRuntimeStats(nowMs);
  updateStatusLed(nowMs);

  if (static_cast<uint32_t>(nowMs - lastHeartbeatMs_) >=
      AppConfig::kHeartbeatIntervalMs) {
    lastHeartbeatMs_ = nowMs;
    printHeartbeat(nowMs, bellStatus);
    checkMemoryWarnings();

    if (AppConfig::kEnableHeartbeatLed) {
      startStatusLedPulse(nowMs, AppConfig::kHeartbeatLedPulseMs);
    }
  }

  if (static_cast<uint32_t>(nowMs - lastDetailedMemoryMs_) >=
      AppConfig::kDetailedMemoryIntervalMs) {
    lastDetailedMemoryMs_ = nowMs;
    printDetailedMemoryReport();
    printBellStatus(bellStatus);
  }
}

RuntimeStats Diagnostics::stats() const {
  return stats_;
}

AppLifecycleState Diagnostics::lifecycleState() const {
  return lifecycleState_;
}

void Diagnostics::log(LogLevel level,
                      const __FlashStringHelper* message) const {
  if (!shouldLog(level)) {
    return;
  }

  Serial.print('[');
  Serial.print(levelText(level));
  Serial.print(F("] "));
  Serial.println(message);
}

void Diagnostics::log(LogLevel level, const char* message) const {
  if (!shouldLog(level)) {
    return;
  }

  Serial.print('[');
  Serial.print(levelText(level));
  Serial.print(F("] "));
  Serial.println(message);
}

void Diagnostics::logBellEvent(const BellEvent& event) const {
  switch (event.type) {
    case BellEventType::BootWaitFinished:
      Serial.print(F("[INFO] DETECTOR | boot_wait_finished | D5="));
      Serial.print(inputLevelText(event.value != 0U));
      Serial.print(F(" | waiting_for_stable_LOW="));
      Serial.print(AppConfig::kArmLowStableMs);
      Serial.println(F("ms"));
      break;

    case BellEventType::Armed:
      Serial.print(F("[INFO] DETECTOR | ARMED | D5=LOW | valid_presses="));
      Serial.println(event.value);
      break;

    case BellEventType::PulseStarted:
      Serial.println(F("[INFO] BELL | pulse_started | LOW->HIGH"));
      break;

    case BellEventType::ValidPress:
      Serial.print(F("[INFO] BELL | VALID | duration="));
      Serial.print(event.pulseDurationUs / 1000U);
      Serial.print(F("ms | total="));
      Serial.println(event.value);
      break;

    case BellEventType::ShortPulseIgnored:
      Serial.print(F("[INFO] BELL | SHORT_IGNORED | duration_us="));
      Serial.print(event.pulseDurationUs);
      Serial.print(F(" | threshold_us="));
      Serial.print(AppConfig::kMinimumBellHighMs * 1000UL);
      Serial.print(F(" | ignored_total="));
      Serial.println(event.value);
      break;

    case BellEventType::HighTooLong:
      Serial.print(F("[WARN] SIGNAL | HIGH_TOO_LONG | duration="));
      Serial.print(event.pulseDurationUs / 1000U);
      Serial.print(F("ms | limit="));
      Serial.print(AppConfig::kMaximumBellHighMs);
      Serial.print(F("ms | total="));
      Serial.println(event.value);
      break;

    case BellEventType::RecoveredFromHigh:
      Serial.print(F("[INFO] SIGNAL | returned_to_LOW | high_duration="));
      Serial.print(event.pulseDurationUs / 1000U);
      Serial.println(F("ms | waiting_for_rearm"));
      break;

    case BellEventType::RawQueueOverflow:
      Serial.print(F("[WARN] INPUT | raw_edge_queue_full | dropped="));
      Serial.println(event.value);
      break;

    case BellEventType::InputResynchronized:
      Serial.print(F("[WARN] INPUT | resynchronized | physical_level="));
      Serial.println(inputLevelText(event.value != 0U));
      break;
  }
}

void Diagnostics::pulseStatusLed(uint32_t durationMs) {
  startStatusLedPulse(millis(), durationMs);
}

void Diagnostics::configureStatusLed() {
  pinMode(AppConfig::kStatusLedPin, OUTPUT);
  setStatusLed(false);
}

void Diagnostics::setStatusLed(bool enabled) {
  const bool outputHigh =
      AppConfig::kStatusLedActiveLow ? !enabled : enabled;
  digitalWrite(AppConfig::kStatusLedPin, outputHigh ? HIGH : LOW);
}

void Diagnostics::startStatusLedPulse(uint32_t nowMs,
                                      uint32_t durationMs) {
  if (ledPulseActive_) {
    const uint32_t elapsed = nowMs - ledPulseStartedMs_;
    const uint32_t remaining =
        elapsed < ledPulseDurationMs_ ? ledPulseDurationMs_ - elapsed : 0U;

    if (remaining > durationMs) {
      durationMs = remaining;
    }
  }

  setStatusLed(true);
  ledPulseStartedMs_ = nowMs;
  ledPulseDurationMs_ = durationMs;
  ledPulseActive_ = true;
}

void Diagnostics::updateStatusLed(uint32_t nowMs) {
  if (!ledPulseActive_) {
    return;
  }

  if (static_cast<uint32_t>(nowMs - ledPulseStartedMs_) >=
      ledPulseDurationMs_) {
    setStatusLed(false);
    ledPulseActive_ = false;
  }
}

void Diagnostics::sampleRuntimeStats(uint32_t nowMs) {
  stats_.uptimeMs = nowMs;
  stats_.freeHeapBytes = ESP.getFreeHeap();
  stats_.maximumFreeBlockBytes = ESP.getMaxFreeBlockSize();
  stats_.heapFragmentationPercent = ESP.getHeapFragmentation();

  if (stats_.minimumFreeHeapBytes == 0 ||
      stats_.freeHeapBytes < stats_.minimumFreeHeapBytes) {
    stats_.minimumFreeHeapBytes = stats_.freeHeapBytes;
  }
}

void Diagnostics::printBootReport() {
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("       COMMAX ESP8266 SMART DOORBELL"));
  Serial.println(F("=================================================="));

  Serial.print(F("Firmware: "));
  Serial.print(AppConfig::kFirmwareName);
  Serial.print(' ');
  Serial.println(AppConfig::kFirmwareVersion);

  Serial.print(F("Implementation: "));
  Serial.println(AppConfig::kImplementationStage);

  Serial.print(F("Built: "));
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);

  Serial.print(F("ESP8266 Arduino Core: "));
  Serial.println(ESP.getCoreVersion());

  Serial.print(F("SDK: "));
  Serial.println(ESP.getSdkVersion());

  Serial.print(F("Reset reason: "));
  Serial.println(ESP.getResetReason());

  Serial.print(F("CPU: "));
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(F(" MHz"));

  Serial.print(F("Physical flash: "));
  Serial.print(bytesToKiB(ESP.getFlashChipRealSize()));
  Serial.println(F(" KiB"));

  Serial.print(F("Sketch size: "));
  Serial.print(bytesToKiB(ESP.getSketchSize()));
  Serial.println(F(" KiB"));

  Serial.print(F("Free sketch space: "));
  Serial.print(bytesToKiB(ESP.getFreeSketchSpace()));
  Serial.println(F(" KiB"));

  printDetailedMemoryReport();

  Serial.println(F("Bell input: D5/GPIO14 with external 10 kOhm pull-up."));
  Serial.println(F("Signal logic: idle=LOW, active=HIGH, event on return to LOW."));
  Serial.print(F("Arming: "));
  Serial.print(AppConfig::kBootWaitMs);
  Serial.print(F("ms startup wait + "));
  Serial.print(AppConfig::kArmLowStableMs);
  Serial.println(F("ms stable LOW."));
  Serial.print(F("Valid HIGH pulse: "));
  Serial.print(AppConfig::kMinimumBellHighMs);
  Serial.print(F("ms to "));
  Serial.print(AppConfig::kMaximumBellHighMs);
  Serial.println(F("ms."));
  Serial.print(F("Rearm: "));
  Serial.print(AppConfig::kRearmLowStableMs);
  Serial.println(F("ms stable LOW."));
  Serial.println(F("Wi-Fi, LittleFS, Imou, and ntfy are not active yet."));
  Serial.println(F("=================================================="));
  Serial.println();
}

void Diagnostics::printHeartbeat(
    uint32_t nowMs,
    const BellDetectorStatus& bellStatus) {
  Serial.print(F("[INFO] HEARTBEAT | uptime="));
  Serial.print(nowMs / 1000U);
  Serial.print(F("s | heap="));
  Serial.print(stats_.freeHeapBytes);
  Serial.print(F(" B | minimum="));
  Serial.print(stats_.minimumFreeHeapBytes);
  Serial.print(F(" B | fragmentation="));
  Serial.print(stats_.heapFragmentationPercent);
  Serial.print(F("% | loops="));
  Serial.print(stats_.loopCount);
  Serial.print(F(" | detector="));
  Serial.print(bellStateText(bellStatus.state));
  Serial.print(F(" | D5="));
  Serial.print(inputLevelText(bellStatus.currentLevelHigh));
  Serial.print(F(" | valid_bells="));
  Serial.println(bellStatus.stats.validPresses);
}

void Diagnostics::printDetailedMemoryReport() {
  Serial.print(F("[INFO] MEMORY | free="));
  Serial.print(stats_.freeHeapBytes);
  Serial.print(F(" B | largest_block="));
  Serial.print(stats_.maximumFreeBlockBytes);
  Serial.print(F(" B | fragmentation="));
  Serial.print(stats_.heapFragmentationPercent);
  Serial.print(F("% | minimum_since_boot="));
  Serial.print(stats_.minimumFreeHeapBytes);
  Serial.println(F(" B"));
}

void Diagnostics::printBellStatus(
    const BellDetectorStatus& bellStatus) const {
  Serial.print(F("[INFO] DETECTOR_STATUS | state="));
  Serial.print(bellStateText(bellStatus.state));
  Serial.print(F(" | D5="));
  Serial.print(inputLevelText(bellStatus.currentLevelHigh));
  Serial.print(F(" | pending_edges="));
  Serial.print(bellStatus.pendingRawEdges);
  Serial.print(F(" | raw_edges="));
  Serial.print(bellStatus.stats.rawEdges);
  Serial.print(F(" | valid="));
  Serial.print(bellStatus.stats.validPresses);
  Serial.print(F(" | short="));
  Serial.print(bellStatus.stats.shortPulsesIgnored);
  Serial.print(F(" | long_high="));
  Serial.print(bellStatus.stats.highTooLongEvents);
  Serial.print(F(" | dropped_edges="));
  Serial.print(bellStatus.stats.rawEdgesDropped);
  Serial.print(F(" | dropped_events="));
  Serial.print(bellStatus.stats.detectorEventsDropped);
  Serial.print(F(" | resyncs="));
  Serial.println(bellStatus.stats.inputResynchronizations);
}

void Diagnostics::checkMemoryWarnings() {
  if (stats_.freeHeapBytes < AppConfig::kHeapWarningBytes) {
    log(LogLevel::Warning, F("Free heap is below the configured threshold."));
  }

  if (stats_.heapFragmentationPercent >
      AppConfig::kHeapFragmentationWarningPercent) {
    log(LogLevel::Warning,
        F("Heap fragmentation exceeded the configured threshold."));
  }
}

bool Diagnostics::shouldLog(LogLevel level) const {
  return static_cast<uint8_t>(level) <=
         static_cast<uint8_t>(AppConfig::kCompileTimeLogLevel);
}

const __FlashStringHelper* Diagnostics::levelText(LogLevel level) const {
  switch (level) {
    case LogLevel::Error:
      return F("ERROR");
    case LogLevel::Warning:
      return F("WARN");
    case LogLevel::Info:
      return F("INFO");
    case LogLevel::Debug:
      return F("DEBUG");
    default:
      return F("LOG");
  }
}

const __FlashStringHelper* Diagnostics::bellStateText(
    BellDetectorState state) const {
  switch (state) {
    case BellDetectorState::BootWait:
      return F("BOOT_WAIT");
    case BellDetectorState::WaitingForStableLow:
      return F("WAITING_FOR_LOW");
    case BellDetectorState::Armed:
      return F("ARMED");
    case BellDetectorState::HighInProgress:
      return F("HIGH_IN_PROGRESS");
    case BellDetectorState::HighDuringRearm:
      return F("HIGH_DURING_REARM");
    case BellDetectorState::HighTooLong:
      return F("HIGH_TOO_LONG");
    case BellDetectorState::WaitingForRearm:
      return F("WAITING_FOR_REARM");
    default:
      return F("UNKNOWN");
  }
}

const __FlashStringHelper* Diagnostics::inputLevelText(
    bool levelHigh) const {
  return levelHigh ? F("HIGH") : F("LOW");
}
