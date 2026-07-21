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
  log(LogLevel::Info, F("Stage 1 initialization completed."));

  lastHeartbeatMs_ = millis();
  lastDetailedMemoryMs_ = millis();
  startStatusLedPulse(millis());
}

void Diagnostics::tick() {
  const uint32_t nowMs = millis();

  ++stats_.loopCount;
  sampleRuntimeStats(nowMs);
  updateStatusLed(nowMs);

  if (static_cast<uint32_t>(nowMs - lastHeartbeatMs_) >=
      AppConfig::kHeartbeatIntervalMs) {
    lastHeartbeatMs_ = nowMs;
    printHeartbeat(nowMs);
    checkMemoryWarnings();

    if (AppConfig::kEnableHeartbeatLed) {
      startStatusLedPulse(nowMs);
    }
  }

  if (static_cast<uint32_t>(nowMs - lastDetailedMemoryMs_) >=
      AppConfig::kDetailedMemoryIntervalMs) {
    lastDetailedMemoryMs_ = nowMs;
    printDetailedMemoryReport();
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

void Diagnostics::configureStatusLed() {
  pinMode(AppConfig::kStatusLedPin, OUTPUT);
  setStatusLed(false);
}

void Diagnostics::setStatusLed(bool enabled) {
  const bool outputHigh =
      AppConfig::kStatusLedActiveLow ? !enabled : enabled;
  digitalWrite(AppConfig::kStatusLedPin, outputHigh ? HIGH : LOW);
}

void Diagnostics::startStatusLedPulse(uint32_t nowMs) {
  setStatusLed(true);
  ledPulseStartedMs_ = nowMs;
  ledPulseActive_ = true;
}

void Diagnostics::updateStatusLed(uint32_t nowMs) {
  if (!ledPulseActive_) {
    return;
  }

  if (static_cast<uint32_t>(nowMs - ledPulseStartedMs_) >=
      AppConfig::kStatusLedPulseMs) {
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
  Serial.print(F(" "));
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

  Serial.println(F("D5 is not configured in this stage."));
  Serial.println(F("Wi-Fi, LittleFS, Imou, and ntfy are not active yet."));
  Serial.println(F("=================================================="));
  Serial.println();
}

void Diagnostics::printHeartbeat(uint32_t nowMs) {
  Serial.print(F("[INFO] HEARTBEAT | uptime="));
  Serial.print(nowMs / 1000U);
  Serial.print(F("s | heap="));
  Serial.print(stats_.freeHeapBytes);
  Serial.print(F(" B | minimum="));
  Serial.print(stats_.minimumFreeHeapBytes);
  Serial.print(F(" B | fragmentation="));
  Serial.print(stats_.heapFragmentationPercent);
  Serial.print(F("% | loops="));
  Serial.println(stats_.loopCount);
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

void Diagnostics::checkMemoryWarnings() {
  if (stats_.freeHeapBytes < AppConfig::kHeapWarningBytes) {
    log(LogLevel::Warning, F("Free heap dropped below the configured threshold."));
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
