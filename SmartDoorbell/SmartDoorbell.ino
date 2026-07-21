#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"
#include "Diagnostics.h"

Diagnostics diagnostics;

void setup() {
  diagnostics.begin();
}

void loop() {
  diagnostics.tick();

  // Give the ESP8266 core time for internal tasks and watchdog servicing.
  yield();
}
