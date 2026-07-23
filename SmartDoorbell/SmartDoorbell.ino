#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"
#include "BellDetector.h"
#include "Diagnostics.h"

Diagnostics diagnostics;
BellDetector bellDetector;

namespace {

void processBellEvents() {
  BellEvent event{};

  while (bellDetector.popEvent(event)) {
    diagnostics.logBellEvent(event);

    if (event.type == BellEventType::ValidPress &&
        AppConfig::kEnableValidBellLed) {
      diagnostics.pulseStatusLed(AppConfig::kValidBellLedPulseMs);
    }
  }
}

}  // namespace

void setup() {
  diagnostics.begin();
  bellDetector.begin();

  const BellDetectorStatus status = bellDetector.status();
  Serial.print(F("[INFO] INPUT | initial_D5="));
  Serial.println(status.currentLevelHigh ? F("HIGH") : F("LOW"));

  diagnostics.log(LogLevel::Info,
                  F("Stage 2 initialization completed."));
}

void loop() {
  // Bell input processing has priority over diagnostics and future services.
  bellDetector.tick();
  processBellEvents();

  diagnostics.tick(bellDetector.status());

  // Yield to the ESP8266 core and feed the software watchdog.
  yield();
}
