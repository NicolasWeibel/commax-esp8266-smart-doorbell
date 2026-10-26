#include <Arduino.h>

#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#error "Copy Secrets.example.h to Secrets.h and configure private values before compiling RC17."
#endif

#include "AppConfig.h"
#include "AppTypes.h"
#include "BellDetector.h"
#include "Diagnostics.h"
#include "DoorbellController.h"

Diagnostics diagnostics;
BellDetector bellDetector;
DoorbellController controller;
String serialCommand;

namespace {

void processBellInput() {
  bellDetector.tick();

  BellEvent event{};
  while (bellDetector.popEvent(event)) {
    diagnostics.logBellEvent(event);

    if (event.type == BellEventType::ValidPress) {
      if (AppConfig::kEnableValidBellLed) {
        diagnostics.pulseStatusLed(AppConfig::kValidBellLedPulseMs);
      }
      controller.onValidBell();
    }
  }
}

bool bellInputActivityInFlight() {
  return bellDetector.activityInFlight();
}

void printConsoleHelp() {
  Serial.println(F("Comandos serie:"));
  Serial.println(F("  help       - mostrar esta ayuda"));
  Serial.println(F("  status     - mostrar sesión y conectividad"));
  Serial.println(F("  sim        - simular un timbre ya validado"));
  Serial.println(F("  sim <1..20>- simular varios timbres validados"));
  Serial.println(F("  diag       - ejecutar diagnóstico de red por capas"));
  Serial.println(F("  diagstatus - mostrar destino local configurado"));
  Serial.println(F("  diagpc <IPv4> <puerto> - configurar servidor local"));
  Serial.println(F("  diagpc off - desactivar prueba TCP local"));
  Serial.println(F("  wifihealth - mostrar estado SDK, DHCP y lwIP"));
  Serial.println(F("  wifievents - mostrar historial circular Wi-Fi"));
  Serial.println(F("  wificonfig - mostrar DHCP y MAC"));
  Serial.println(F("  wifireconnect - reciclar interfaz Wi-Fi ahora"));
  Serial.println(F("  coredebug on|off - salida interna del Core"));
}

void printControllerStatus() {
  const SessionStatus session = controller.sessionStatus();
  const ConnectivityStatus connection = controller.connectivityStatus();
  Serial.print(F("[STATUS] session="));
  Serial.print(session.active ? F("active") : F("idle"));
  Serial.print(F(" | phase="));
  switch (session.phase) {
    case SessionPhase::Idle:
      Serial.print(F("Idle"));
      break;
    case SessionPhase::FirstMinute:
      Serial.print(F("FirstMinute"));
      break;
    case SessionPhase::FirstFinalPending:
      Serial.print(F("FirstFinalPending"));
      break;
    case SessionPhase::Insistence:
      Serial.print(F("Insistence"));
      break;
  }
  Serial.print(F(" | presses="));
  Serial.print(session.totalPresses);
  Serial.print(F(" | connectivity="));
  switch (connection.state) {
    case ConnectivityState::WaitingForTime:
      Serial.print(F("WAITING_FOR_TIME"));
      break;
    case ConnectivityState::Online:
      Serial.print(F("ONLINE"));
      break;
    case ConnectivityState::Degraded:
      Serial.print(F("DEGRADED"));
      break;
    case ConnectivityState::Offline:
      Serial.print(F("OFFLINE"));
      break;
    case ConnectivityState::Recovering:
      Serial.print(F("RECOVERING"));
      break;
  }
  const OfflineSummaryStatus offline = controller.offlineSummaryStatus();
  Serial.print(F(" | failures="));
  Serial.print(connection.consecutiveFailures);
  Serial.print(F(" | offline_presses="));
  Serial.print(offline.totalPresses);
  Serial.print(F(" | offline_occasions="));
  Serial.println(offline.occasionCount);
}

void executeSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command.length() == 0U) {
    return;
  }

  if (command == F("help")) {
    printConsoleHelp();
    return;
  }
  if (command == F("status")) {
    printControllerStatus();
    return;
  }
  if (command == F("sim")) {
    controller.onValidBell();
    Serial.println(F("[INFO] SIMULATION | validated_bells=1"));
    return;
  }
  if (command == F("diag")) {
    controller.runNetworkDiagnostics(processBellInput, false);
    return;
  }
  if (command == F("diagstatus")) {
    controller.printDiagnosticConfiguration();
    return;
  }
  if (command == F("diagpc off")) {
    controller.clearDiagnosticPc();
    Serial.println(F("[INFO] NET_DIAG_CONFIG | local_target=disabled"));
    return;
  }
  if (command.startsWith(F("diagpc "))) {
    String arguments = command.substring(7);
    arguments.trim();
    const int separator = arguments.indexOf(' ');
    if (separator <= 0) {
      Serial.println(F("[ERROR] NET_DIAG_CONFIG | use diagpc <IPv4> <port>"));
      return;
    }
    const String ipText = arguments.substring(0, separator);
    const long portValue = arguments.substring(separator + 1).toInt();
    if (portValue < 1 || portValue > 65535 ||
        !controller.setDiagnosticPc(ipText,
                                    static_cast<uint16_t>(portValue))) {
      Serial.println(F("[ERROR] NET_DIAG_CONFIG | invalid IPv4 or port"));
      return;
    }
    controller.printDiagnosticConfiguration();
    return;
  }
  if (command == F("wifihealth")) {
    controller.printWifiSnapshot();
    return;
  }
  if (command == F("wifievents")) {
    controller.printWifiHistory();
    return;
  }
  if (command == F("wificonfig")) {
    controller.printWifiConfiguration();
    return;
  }
  if (command == F("wifireconnect")) {
    const bool started = controller.forceWifiReconnect();
    Serial.print(F("[INFO] WIFI_COMMAND | reconnect="));
    Serial.println(started ? F("started") : F("failed"));
    return;
  }
  if (command == F("coredebug on")) {
    Serial.setDebugOutput(true);
    Serial.println(F("[INFO] CORE_DEBUG | enabled=yes | requires IDE Debug Port/Level compilation options"));
    return;
  }
  if (command == F("coredebug off")) {
    Serial.setDebugOutput(false);
    Serial.println(F("[INFO] CORE_DEBUG | enabled=no"));
    return;
  }

  if (command.startsWith(F("sim "))) {
    const long requested = command.substring(4).toInt();
    if (requested < 1 || requested > 20) {
      Serial.println(F("[ERROR] SIMULATION | use sim <1..20>"));
      return;
    }
    for (long i = 0; i < requested; ++i) {
      controller.onValidBell();
      controller.tick(processBellInput, bellInputActivityInFlight);
      delay(100);
    }
    Serial.print(F("[INFO] SIMULATION | validated_bells="));
    Serial.println(requested);
    return;
  }

  Serial.println(F("[ERROR] Comando desconocido. Escribí help."));
}

void processSerialConsole() {
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) {
      break;
    }
    const char character = static_cast<char>(value);
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      executeSerialCommand(serialCommand);
      serialCommand.remove(0);
      continue;
    }
    if (serialCommand.length() < 80U) {
      serialCommand += character;
    }
  }
}

}  // namespace

void setup() {
  diagnostics.begin();
  serialCommand.reserve(80);
  bellDetector.begin();

  const BellDetectorStatus status = bellDetector.status();
  Serial.print(F("[INFO] INPUT | initial_D5="));
  Serial.println(status.currentLevelHigh ? F("HIGH") : F("LOW"));

  controller.begin();
  diagnostics.log(LogLevel::Info,
                  F("RC17 production text-only initialization completed."));
  Serial.println(F("[INFO] CONSOLE | escribi help para ver comandos"));
}

void loop() {
  // The timing-sensitive bell path is always serviced before network work.
  processBellInput();
  processSerialConsole();

  // Network operations remain sequential. The ISR keeps collecting D5
  // transitions while the controller services ntfy and recovery tasks.
  controller.tick(processBellInput, bellInputActivityInFlight);

  processBellInput();
  processSerialConsole();
  diagnostics.tick(bellDetector.status());
  yield();
}
