#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>

#include "TlsManager.h"

struct NetworkProbeResult {
  bool attempted = false;
  bool success = false;
  uint32_t durationMs = 0;
  int sslError = 0;
  IPAddress address{};
};

class NetworkDiagnostics {
 public:
  using InputPump = void (*)();

  explicit NetworkDiagnostics(TlsManager& tlsManager)
      : tlsManager_(tlsManager) {}

  bool setLocalTarget(const String& ipv4Text, uint16_t port);
  void clearLocalTarget();
  bool localTargetConfigured() const;
  void printConfiguration() const;

  bool automaticDue(uint64_t nowMs) const;
  void run(uint64_t nowMs,
           const String& ntfyHost,
           uint16_t ntfyPort,
           bool timeValid,
           time_t currentEpoch,
           InputPump inputPump,
           bool automatic,
           const __FlashStringHelper* triggerName = nullptr);

 private:
  NetworkProbeResult resolveHost(const String& host,
                                 InputPump inputPump) const;
  NetworkProbeResult probeTcp(const IPAddress& address,
                              uint16_t port,
                              InputPump inputPump,
                              bool sendLocalMarker) const;
  NetworkProbeResult probeTls(const String& host,
                              uint16_t port,
                              bool timeValid,
                              time_t currentEpoch,
                              InputPump inputPump) const;
  void printProbe(const __FlashStringHelper* name,
                  const NetworkProbeResult& result,
                  uint16_t port = 0) const;
  void printClassification(bool wifiConnected,
                           bool ntfyDns,
                           bool alternateDns,
                           bool localAttempted,
                           bool localTcp,
                           bool ntfyTcp,
                           bool alternateTcp,
                           bool ntfyTls,
                           bool alternateTls) const;
  void cooperativePause(uint32_t durationMs, InputPump inputPump) const;

  TlsManager& tlsManager_;
  IPAddress localTargetIp_{};
  uint16_t localTargetPort_ = 0;
  bool localTargetConfigured_ = false;
  uint64_t lastAutomaticRunMs_ = 0;
};
