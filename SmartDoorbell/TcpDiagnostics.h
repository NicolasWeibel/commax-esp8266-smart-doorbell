#pragma once

#include <Arduino.h>

struct TcpDiagnosticSnapshot {
  uint16_t activePcbs = 0;
  uint16_t timeWaitPcbs = 0;
  uint16_t boundPcbs = 0;
  uint16_t listenPcbs = 0;
};

class TcpDiagnostics {
 public:
  static TcpDiagnosticSnapshot snapshot();
};
