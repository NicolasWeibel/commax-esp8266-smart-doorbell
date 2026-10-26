#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "WifiOutageAggregator.h"

extern "C" {
#include <lwip/netif.h>
}

enum class WifiTraceEventType : uint8_t {
  Begin = 0,
  ConnectionAttempt,
  ForcedRecycle,
  StationConnected,
  StationDisconnected,
  GotIp,
  DhcpTimeout,
  ModeChanged,
  AuthModeChanged,
  NetifLinkChanged,
  NetifStatusChanged,
  NetifIpv4AddressChanged,
  NetifIpv4GatewayChanged,
  NetifIpv4NetmaskChanged,
  NetifIpv4SettingsChanged,
  Ipv4BecameInvalid,
  Ipv4Recovered,
  ApUnavailableSummary,
  WifiRecoveredSummary,
};

struct WifiRuntimeSnapshot {
  wl_status_t arduinoStatus = WL_NO_SHIELD;
  uint8_t sdkStationStatus = 0;
  uint8_t sdkDhcpStatus = 0;
  bool associated = false;
  bool ipv4Valid = false;
  bool netifFound = false;
  bool netifUp = false;
  bool netifLinkUp = false;
  bool dhcpSuppliedAddress = false;
  uint8_t dhcpState = 0xFF;
  uint8_t dhcpTries = 0;
  uint16_t dhcpLeaseUsedMinutes = 0;
  uint16_t dhcpLeaseMinutes = 0;
  uint16_t dhcpRenewMinutes = 0;
  uint16_t dhcpRebindMinutes = 0;
  int32_t rssi = 0;
  uint8_t channel = 0;
  IPAddress localIp{};
  IPAddress gateway{};
  IPAddress subnet{};
  IPAddress dns{};
};

struct WifiTraceEvent {
  uint32_t sequence = 0;
  uint32_t uptimeMs = 0;
  WifiTraceEventType type = WifiTraceEventType::Begin;
  int16_t reason = 0;
  uint16_t detail = 0;
  uint8_t arduinoStatus = 0;
  uint8_t sdkStationStatus = 0;
  uint8_t sdkDhcpStatus = 0;
  uint8_t netifFlags = 0;
  int16_t rssi = 0;
  uint32_t localIp = 0;
  uint32_t gateway = 0;
  uint32_t subnet = 0;
};

class WifiManager {
 public:
  void begin(uint64_t nowMs);
  void tick(uint64_t nowMs);
  bool forceReconnect(uint64_t nowMs, bool bypassCooldown = false);

  // connected() means usable IPv4 connectivity, not merely association to AP.
  bool connected() const;
  bool associated() const;
  bool ipv4Valid() const;
  wl_status_t status() const;
  IPAddress localIp() const;
  WifiRuntimeSnapshot snapshot() const;

  bool consumeInvalidIpv4Detected();
  bool recoverInvalidIpv4(uint64_t nowMs);

  void printSnapshot(const __FlashStringHelper* trigger) const;
  void printHistory() const;
  void printConfiguration() const;
  void printFaultAssessment() const;

 private:
  static constexpr uint8_t kTraceCapacity = 32;

  void registerEventHandlers();
  void startConnection(uint64_t nowMs);
  void applyDhcpConfiguration();
  void inspectHealth(uint64_t nowMs);
  void updateNoApAggregation(uint64_t nowMs);
  void flushNewEvents();
  void recordEvent(WifiTraceEventType type,
                   int16_t reason = 0,
                   uint16_t detail = 0);
  void recordCallbackEvent(WifiTraceEventType type,
                           int16_t reason,
                           uint16_t detail,
                           const IPAddress& localIp = IPAddress(),
                           const IPAddress& gateway = IPAddress(),
                           const IPAddress& subnet = IPAddress());
  void recordEventWithSnapshot(WifiTraceEventType type,
                               int16_t reason,
                               uint16_t detail,
                               const WifiRuntimeSnapshot& current);
  const WifiTraceEvent* traceBySequence(uint32_t sequence) const;
  const WifiTraceEvent* latestRelevantFaultEvent() const;
  struct netif* stationNetif() const;

  static const __FlashStringHelper* eventName(WifiTraceEventType type);
  static const __FlashStringHelper* disconnectReasonName(int reason);
  static const __FlashStringHelper* stationStatusName(uint8_t status);
  static const __FlashStringHelper* dhcpStatusName(uint8_t status);

  WiFiEventHandler stationConnectedHandler_{};
  WiFiEventHandler stationDisconnectedHandler_{};
  WiFiEventHandler gotIpHandler_{};
  WiFiEventHandler dhcpTimeoutHandler_{};
  WiFiEventHandler modeChangedHandler_{};
  WiFiEventHandler authModeChangedHandler_{};

  WifiTraceEvent trace_[kTraceCapacity]{};
  WifiRuntimeSnapshot previousSnapshot_{};
  bool previousSnapshotValid_ = false;
  uint32_t nextTraceSequence_ = 1;
  uint32_t nextPrintSequence_ = 1;
  uint32_t overwrittenEvents_ = 0;

  uint64_t lastAttemptMs_ = 0;
  bool started_ = false;
  uint64_t lastForcedReconnectMs_ = 0;
  uint64_t lastHealthPollMs_ = 0;
  uint64_t invalidIpv4SinceMs_ = 0;
  bool invalidIpv4Observed_ = false;
  bool invalidIpv4NotificationPending_ = false;
  bool intentionalDisconnect_ = false;
  uint8_t invalidIpv4RecoverySeriesCount_ = 0;
  uint64_t nextInvalidIpv4RecoveryAllowedMs_ = 0;
  uint64_t validIpv4SinceMs_ = 0;

  WifiOutageAggregator noApOutage_{};
};
