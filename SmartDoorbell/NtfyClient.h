#pragma once

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "AppTypes.h"
#include "TlsManager.h"

class NtfyClient {
 public:
  using InputPump = void (*)();

  explicit NtfyClient(TlsManager& tlsManager) : tlsManager_(tlsManager) {}
  ~NtfyClient();

  bool begin();
  void setEnvironment(bool wifiConnected,
                      bool timeValid,
                      time_t currentEpoch);
  void tick(uint64_t nowMs, bool wifiConnected);
  void close();
  const String& host() const;
  uint16_t port() const;

  NtfyPublishResult publish(const DeliveryAttempt& attempt,
                            const String& title,
                            const String& message,
                            InputPump inputPump = nullptr);

 private:
  bool validateEndpoint(const String& baseUrl);
  bool validateEnvironment(NtfyPublishResult& result) const;
  String publishPath(const char* sequenceId) const;
  String priority(const DeliveryAttempt& attempt) const;

  bool ensureTransport(const String& path, NtfyPublishResult& result);
  void configureHttpClient();
  void addRequestHeaders(const DeliveryAttempt& attempt,
                         const String& title);
  NtfyPublishResult postOnce(const DeliveryAttempt& attempt,
                             const String& title,
                             const String& message,
                             const String& path,
                             InputPump inputPump);
  bool isReconnectableFailure(NtfyResultCode code) const;
  void resetTransport(const __FlashStringHelper* reason,
                      bool abortSocket,
                      bool logClose = true);
  void cooperativeDelay(uint32_t durationMs, InputPump inputPump) const;
  NtfyResultCode classifyHttpClientError(int errorCode,
                                         int sslError) const;
  int captureSslError(String& destination) const;

  TlsManager& tlsManager_;
  BearSSL::WiFiClientSecure secureClient_{};
  HTTPClient http_{};

  String host_{};
  String basePath_{};
  uint16_t port_ = 443;
  bool endpointReady_ = false;
  bool httpReady_ = false;
  bool wifiConnected_ = false;
  bool timeValid_ = false;
  time_t currentEpoch_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t transportGeneration_ = 0;
  uint8_t consecutiveConnectFailures_ = 0;
};
