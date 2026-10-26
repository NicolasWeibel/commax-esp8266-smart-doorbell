#include "WifiManager.h"

extern "C" {
#include <lwip/dhcp.h>
#include <user_interface.h>
}

#include "AppConfig.h"
#include "Secrets.h"

namespace {

uint32_t ipToRaw(const IPAddress& address) {
  return static_cast<uint32_t>(address);
}

bool validAddress(const IPAddress& address) {
  return ipToRaw(address) != 0U;
}

const __FlashStringHelper* yesNo(bool value) {
  return value ? F("yes") : F("no");
}

uint32_t recoveryBackoffMs(uint8_t completedRecoveries) {
  if (completedRecoveries <= 1U) {
    return AppConfig::kWifiRecoveryBackoffAfter1Ms;
  }
  if (completedRecoveries == 2U) {
    return AppConfig::kWifiRecoveryBackoffAfter2Ms;
  }
  return AppConfig::kWifiRecoveryBackoffAfter3Ms;
}

int16_t cappedAttempts(uint32_t attempts) {
  return attempts > 32767U ? 32767 : static_cast<int16_t>(attempts);
}

uint16_t cappedDurationSeconds(uint32_t durationMs) {
  const uint32_t seconds = durationMs / 1000U;
  return seconds > 65535U ? 65535U : static_cast<uint16_t>(seconds);
}

bool importantLiveEvent(WifiTraceEventType type) {
  switch (type) {
    case WifiTraceEventType::ForcedRecycle:
    case WifiTraceEventType::StationConnected:
    case WifiTraceEventType::StationDisconnected:
    case WifiTraceEventType::GotIp:
    case WifiTraceEventType::DhcpTimeout:
    case WifiTraceEventType::Ipv4BecameInvalid:
    case WifiTraceEventType::Ipv4Recovered:
    case WifiTraceEventType::ApUnavailableSummary:
    case WifiTraceEventType::WifiRecoveredSummary:
      return true;
    default:
      return false;
  }
}

}  // namespace

void WifiManager::begin(uint64_t nowMs) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(Secrets::kWifiHostname);
  WiFi.setAutoReconnect(true);
  registerEventHandlers();
  recordEvent(WifiTraceEventType::Begin);
  startConnection(nowMs);
}

void WifiManager::tick(uint64_t nowMs) {
  inspectHealth(nowMs);
  updateNoApAggregation(nowMs);
  flushNewEvents();

  if (connected() || invalidIpv4Observed_) {
    return;
  }

  if (!started_ ||
      (nowMs - lastAttemptMs_) >= AppConfig::kWifiReconnectIntervalMs) {
    startConnection(nowMs);
  }
}

bool WifiManager::forceReconnect(uint64_t nowMs, bool bypassCooldown) {
  if (!bypassCooldown && lastForcedReconnectMs_ != 0U &&
      (nowMs - lastForcedReconnectMs_) <
          AppConfig::kWifiForcedRecycleCooldownMs) {
    return false;
  }

  lastForcedReconnectMs_ = nowMs;
  lastAttemptMs_ = nowMs;
  started_ = true;
  intentionalDisconnect_ = true;
  recordEvent(WifiTraceEventType::ForcedRecycle,
              bypassCooldown ? 1 : 0,
              0);

  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(50);
  yield();
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(Secrets::kWifiHostname);
  WiFi.setAutoReconnect(true);
  applyDhcpConfiguration();
  WiFi.begin(Secrets::kWifiSsid, Secrets::kWifiPassword);

  invalidIpv4SinceMs_ = 0;
  invalidIpv4Observed_ = false;
  invalidIpv4NotificationPending_ = false;
  validIpv4SinceMs_ = 0;
  return true;
}

bool WifiManager::connected() const {
  return associated() && ipv4Valid();
}

bool WifiManager::associated() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::ipv4Valid() const {
  return validAddress(WiFi.localIP()) && validAddress(WiFi.gatewayIP()) &&
         validAddress(WiFi.subnetMask());
}

wl_status_t WifiManager::status() const {
  return WiFi.status();
}

IPAddress WifiManager::localIp() const {
  return WiFi.localIP();
}

WifiRuntimeSnapshot WifiManager::snapshot() const {
  WifiRuntimeSnapshot current{};
  current.arduinoStatus = WiFi.status();
  current.sdkStationStatus =
      static_cast<uint8_t>(wifi_station_get_connect_status());
  current.sdkDhcpStatus =
      static_cast<uint8_t>(wifi_station_dhcpc_status());
  current.associated = current.arduinoStatus == WL_CONNECTED;
  current.localIp = WiFi.localIP();
  current.gateway = WiFi.gatewayIP();
  current.subnet = WiFi.subnetMask();
  current.dns = WiFi.dnsIP(0);
  current.ipv4Valid = validAddress(current.localIp) &&
                      validAddress(current.gateway) &&
                      validAddress(current.subnet);
  current.rssi = current.associated ? WiFi.RSSI() : 0;
  current.channel = current.associated ? WiFi.channel() : 0;

  struct netif* interface = stationNetif();
  if (interface != nullptr) {
    current.netifFound = true;
    current.netifUp = netif_is_up(interface) != 0;
    current.netifLinkUp = netif_is_link_up(interface) != 0;
#if LWIP_DHCP
    struct dhcp* dhcpData = netif_dhcp_data(interface);
    current.dhcpSuppliedAddress = dhcp_supplied_address(interface) != 0;
    if (dhcpData != nullptr) {
      current.dhcpState = dhcpData->state;
      current.dhcpTries = dhcpData->tries;
      current.dhcpLeaseUsedMinutes = dhcpData->lease_used;
      current.dhcpLeaseMinutes = dhcpData->t0_timeout;
      current.dhcpRenewMinutes = dhcpData->t1_timeout;
      current.dhcpRebindMinutes = dhcpData->t2_timeout;
    }
#endif
  }
  return current;
}

bool WifiManager::consumeInvalidIpv4Detected() {
  const bool pending = invalidIpv4NotificationPending_;
  invalidIpv4NotificationPending_ = false;
  return pending;
}

bool WifiManager::recoverInvalidIpv4(uint64_t nowMs) {
  if (!invalidIpv4Observed_ ||
      nowMs < nextInvalidIpv4RecoveryAllowedMs_) {
    return false;
  }

  if (!forceReconnect(nowMs, true)) {
    return false;
  }

  if (invalidIpv4RecoverySeriesCount_ < UINT8_MAX) {
    ++invalidIpv4RecoverySeriesCount_;
  }
  const uint32_t backoff =
      recoveryBackoffMs(invalidIpv4RecoverySeriesCount_);
  nextInvalidIpv4RecoveryAllowedMs_ = nowMs + backoff;

  Serial.print(F("[WARN] WIFI_RECOVERY | series="));
  Serial.print(invalidIpv4RecoverySeriesCount_);
  Serial.print(F(" | next_allowed_in_ms="));
  Serial.println(backoff);
  return true;
}

void WifiManager::printSnapshot(const __FlashStringHelper* trigger) const {
  const WifiRuntimeSnapshot current = snapshot();
  Serial.print(F("[WARN] WIFI_SNAPSHOT | trigger="));
  Serial.print(trigger);
  Serial.print(F(" | arduino="));
  Serial.print(static_cast<int>(current.arduinoStatus));
  Serial.print(F(" | associated="));
  Serial.print(yesNo(current.associated));
  Serial.print(F(" | sdk="));
  Serial.print(stationStatusName(current.sdkStationStatus));
  Serial.print(F("("));
  Serial.print(current.sdkStationStatus);
  Serial.print(F(") | dhcp="));
  Serial.print(dhcpStatusName(current.sdkDhcpStatus));
  Serial.print(F("("));
  Serial.print(current.sdkDhcpStatus);
  Serial.print(F(") | mode="));
  Serial.print(F("dhcp"));
  Serial.print(F(" | ip="));
  Serial.print(current.localIp);
  Serial.print(F(" | gateway="));
  Serial.print(current.gateway);
  Serial.print(F(" | subnet="));
  Serial.print(current.subnet);
  Serial.print(F(" | dns="));
  Serial.print(current.dns);
  Serial.print(F(" | ipv4_valid="));
  Serial.print(yesNo(current.ipv4Valid));
  Serial.print(F(" | netif="));
  Serial.print(yesNo(current.netifFound));
  Serial.print(F(" | netif_up="));
  Serial.print(yesNo(current.netifUp));
  Serial.print(F(" | link_up="));
  Serial.print(yesNo(current.netifLinkUp));
  Serial.print(F(" | dhcp_supplied="));
  Serial.print(yesNo(current.dhcpSuppliedAddress));
  Serial.print(F(" | dhcp_state="));
  if (current.dhcpState == 0xFFU) {
    Serial.print(F("none"));
  } else {
    Serial.print(current.dhcpState);
  }
  Serial.print(F(" | dhcp_tries="));
  Serial.print(current.dhcpTries);
  Serial.print(F(" | lease_used_min="));
  Serial.print(current.dhcpLeaseUsedMinutes);
  Serial.print(F(" | lease_total_min="));
  Serial.print(current.dhcpLeaseMinutes);
  Serial.print(F(" | renew_min="));
  Serial.print(current.dhcpRenewMinutes);
  Serial.print(F(" | rebind_min="));
  Serial.print(current.dhcpRebindMinutes);
  Serial.print(F(" | rssi="));
  Serial.print(current.rssi);
  Serial.print(F(" | channel="));
  Serial.println(current.channel);
}

void WifiManager::printHistory() const {
  const uint32_t firstAvailable =
      nextTraceSequence_ > kTraceCapacity
          ? nextTraceSequence_ - kTraceCapacity
          : 1U;
  Serial.print(F("[WARN] WIFI_HISTORY | begin | first_seq="));
  Serial.print(firstAvailable);
  Serial.print(F(" | next_seq="));
  Serial.print(nextTraceSequence_);
  Serial.print(F(" | overwritten="));
  Serial.println(overwrittenEvents_);

  for (uint32_t sequence = firstAvailable;
       sequence < nextTraceSequence_;
       ++sequence) {
    const WifiTraceEvent* event = traceBySequence(sequence);
    if (event == nullptr) {
      continue;
    }
    if (event->type == WifiTraceEventType::ApUnavailableSummary ||
        event->type == WifiTraceEventType::WifiRecoveredSummary) {
      const uint32_t attempts = event->reason > 0
                                    ? static_cast<uint32_t>(event->reason)
                                    : 0U;
      Serial.print(F("[WARN] WIFI_HISTORY_OUTAGE | seq="));
      Serial.print(event->sequence);
      Serial.print(F(" | state="));
      Serial.print(event->type == WifiTraceEventType::WifiRecoveredSummary
                       ? F("recovered")
                       : F("still_offline"));
      Serial.print(F(" | duration_s="));
      Serial.print(event->detail);
      Serial.print(F(" | no_ap_attempts="));
      Serial.print(attempts);
      Serial.print(F(" | suppressed="));
      Serial.print(attempts > 0U ? attempts - 1U : 0U);
      Serial.print(F(" | ip="));
      Serial.println(IPAddress(event->localIp));
      continue;
    }
    Serial.print(F("[WARN] WIFI_HISTORY_EVENT | seq="));
    Serial.print(event->sequence);
    Serial.print(F(" | uptime_ms="));
    Serial.print(event->uptimeMs);
    Serial.print(F(" | event="));
    Serial.print(eventName(event->type));
    Serial.print(F(" | reason="));
    Serial.print(event->reason);
    if (event->type == WifiTraceEventType::StationDisconnected) {
      Serial.print(F("("));
      Serial.print(disconnectReasonName(event->reason));
      Serial.print(F(")"));
    }
    Serial.print(F(" | detail="));
    Serial.print(event->detail);
    Serial.print(F(" | arduino="));
    Serial.print(event->arduinoStatus);
    Serial.print(F(" | sdk="));
    Serial.print(event->sdkStationStatus);
    Serial.print(F(" | dhcp="));
    Serial.print(event->sdkDhcpStatus);
    Serial.print(F(" | netif_flags=0x"));
    Serial.print(event->netifFlags, HEX);
    Serial.print(F(" | ip="));
    Serial.print(IPAddress(event->localIp));
    Serial.print(F(" | gateway="));
    Serial.print(IPAddress(event->gateway));
    Serial.print(F(" | subnet="));
    Serial.print(IPAddress(event->subnet));
    Serial.print(F(" | rssi="));
    Serial.println(event->rssi);
  }
  Serial.println(F("[WARN] WIFI_HISTORY | end"));
}

void WifiManager::printConfiguration() const {
  Serial.print(F("[INFO] WIFI_CONFIG | mode=dhcp | mac="));
  Serial.print(WiFi.macAddress());
  Serial.println(F(" | reservation=router_managed"));
}

void WifiManager::printFaultAssessment() const {
  const WifiRuntimeSnapshot current = snapshot();
  const WifiTraceEvent* relevant = latestRelevantFaultEvent();

  const __FlashStringHelper* classification = F("INCONCLUSIVE");
  const __FlashStringHelper* confidence = F("medium");

  if (relevant != nullptr &&
             relevant->type == WifiTraceEventType::DhcpTimeout) {
    classification = F("DHCP_TIMEOUT_OR_ROUTER_DHCP_NO_RESPONSE");
    confidence = F("high");
  } else if (relevant != nullptr &&
             relevant->type == WifiTraceEventType::StationDisconnected) {
    classification = F("AP_RF_OR_ASSOCIATION_DISCONNECT");
    confidence = F("high");
  } else if (relevant != nullptr &&
             (relevant->type ==
                  WifiTraceEventType::NetifIpv4AddressChanged ||
              relevant->type ==
                  WifiTraceEventType::NetifIpv4GatewayChanged ||
              relevant->type ==
                  WifiTraceEventType::NetifIpv4SettingsChanged)) {
    classification = F("DHCP_OR_LWIP_REMOVED_IPV4_SETTINGS");
    confidence = F("high");
  } else if (current.associated && !current.ipv4Valid &&
             current.sdkStationStatus == STATION_GOT_IP) {
    classification = F("SDK_LWIP_STATE_INCONSISTENCY");
    confidence = F("high");
  } else if (current.associated && !current.ipv4Valid) {
    classification = F("ASSOCIATED_WITHOUT_VALID_IPV4");
    confidence = F("high");
  }

  Serial.print(F("[WARN] WIFI_ROOT_CAUSE | classification="));
  Serial.print(classification);
  Serial.print(F(" | confidence="));
  Serial.print(confidence);
  Serial.print(F(" | ip_mode=dhcp | latest_event="));
  if (relevant == nullptr) {
    Serial.print(F("none"));
  } else {
    Serial.print(eventName(relevant->type));
    Serial.print(F(" | latest_reason="));
    Serial.print(relevant->reason);
    if (relevant->type == WifiTraceEventType::StationDisconnected) {
      Serial.print(F("("));
      Serial.print(disconnectReasonName(relevant->reason));
      Serial.print(F(")"));
    }
    Serial.print(F(" | event_age_ms="));
    Serial.print(millis() - relevant->uptimeMs);
  }
  Serial.println();
}

void WifiManager::registerEventHandlers() {
  stationConnectedHandler_ = WiFi.onStationModeConnected(
      [this](const WiFiEventStationModeConnected& event) {
        intentionalDisconnect_ = false;
        recordCallbackEvent(WifiTraceEventType::StationConnected,
                            0,
                            event.channel);
      });

  stationDisconnectedHandler_ = WiFi.onStationModeDisconnected(
      [this](const WiFiEventStationModeDisconnected& event) {
        const uint16_t detail = intentionalDisconnect_ ? 1U : 0U;
        if (event.reason == WIFI_DISCONNECT_REASON_NO_AP_FOUND &&
            !noApOutage_.recordNoAp(millis())) {
          return;
        }
        recordCallbackEvent(WifiTraceEventType::StationDisconnected,
                            static_cast<int16_t>(event.reason),
                            detail);
      });

  gotIpHandler_ = WiFi.onStationModeGotIP(
      [this](const WiFiEventStationModeGotIP& event) {
        intentionalDisconnect_ = false;
        recordCallbackEvent(WifiTraceEventType::GotIp,
                            0,
                            0,
                            event.ip,
                            event.gw,
                            event.mask);
      });

  dhcpTimeoutHandler_ = WiFi.onStationModeDHCPTimeout([this]() {
    recordCallbackEvent(WifiTraceEventType::DhcpTimeout, 0, 0);
  });

  modeChangedHandler_ = WiFi.onWiFiModeChange(
      [this](const WiFiEventModeChange& event) {
        const uint16_t detail =
            (static_cast<uint16_t>(event.oldMode) << 8U) |
            static_cast<uint16_t>(event.newMode);
        recordCallbackEvent(WifiTraceEventType::ModeChanged, 0, detail);
      });

  authModeChangedHandler_ = WiFi.onStationModeAuthModeChanged(
      [this](const WiFiEventStationModeAuthModeChanged& event) {
        const uint16_t detail =
            (static_cast<uint16_t>(event.oldMode) << 8U) |
            static_cast<uint16_t>(event.newMode);
        recordCallbackEvent(WifiTraceEventType::AuthModeChanged, 0, detail);
      });
}

void WifiManager::startConnection(uint64_t nowMs) {
  lastAttemptMs_ = nowMs;
  started_ = true;
  intentionalDisconnect_ = true;
  if (!noApOutage_.active()) {
    recordEvent(WifiTraceEventType::ConnectionAttempt, 0, 0);
  }
  WiFi.disconnect(false);
  applyDhcpConfiguration();
  WiFi.begin(Secrets::kWifiSsid, Secrets::kWifiPassword);
}

void WifiManager::applyDhcpConfiguration() {
  WiFi.config(0U, 0U, 0U);
}

void WifiManager::updateNoApAggregation(uint64_t nowMs) {
  if (!noApOutage_.active()) {
    return;
  }

  const uint32_t now32 = static_cast<uint32_t>(nowMs);
  if (connected()) {
    const WifiOutageAggregate summary = noApOutage_.finish(now32);
    recordEventWithSnapshot(WifiTraceEventType::WifiRecoveredSummary,
                            cappedAttempts(summary.totalNoApAttempts),
                            cappedDurationSeconds(summary.durationMs),
                            snapshot());
    return;
  }

  if (!noApOutage_.periodicDue(
          now32, AppConfig::kWifiOutageSummaryIntervalMs)) {
    return;
  }

  const WifiOutageAggregate summary = noApOutage_.periodicSummary(now32);
  recordEventWithSnapshot(WifiTraceEventType::ApUnavailableSummary,
                          cappedAttempts(summary.totalNoApAttempts),
                          cappedDurationSeconds(summary.durationMs),
                          snapshot());
}

void WifiManager::inspectHealth(uint64_t nowMs) {
  if (lastHealthPollMs_ != 0U &&
      (nowMs - lastHealthPollMs_) < AppConfig::kWifiHealthPollIntervalMs) {
    return;
  }
  lastHealthPollMs_ = nowMs;

  const WifiRuntimeSnapshot current = snapshot();

  if (previousSnapshotValid_) {
    bool ipv4SettingsChanged = false;
    if (previousSnapshot_.netifUp != current.netifUp) {
      recordEventWithSnapshot(WifiTraceEventType::NetifStatusChanged,
                              current.netifUp ? 1 : 0,
                              previousSnapshot_.netifUp ? 1U : 0U,
                              current);
    }
    if (previousSnapshot_.netifLinkUp != current.netifLinkUp) {
      recordEventWithSnapshot(WifiTraceEventType::NetifLinkChanged,
                              current.netifLinkUp ? 1 : 0,
                              previousSnapshot_.netifLinkUp ? 1U : 0U,
                              current);
    }
    if (ipToRaw(previousSnapshot_.localIp) != ipToRaw(current.localIp)) {
      recordEventWithSnapshot(WifiTraceEventType::NetifIpv4AddressChanged,
                              0,
                              validAddress(previousSnapshot_.localIp) ? 1U : 0U,
                              current);
      ipv4SettingsChanged = true;
    }
    if (ipToRaw(previousSnapshot_.gateway) != ipToRaw(current.gateway)) {
      recordEventWithSnapshot(WifiTraceEventType::NetifIpv4GatewayChanged,
                              0,
                              validAddress(previousSnapshot_.gateway) ? 1U : 0U,
                              current);
      ipv4SettingsChanged = true;
    }
    if (ipToRaw(previousSnapshot_.subnet) != ipToRaw(current.subnet)) {
      recordEventWithSnapshot(WifiTraceEventType::NetifIpv4NetmaskChanged,
                              0,
                              validAddress(previousSnapshot_.subnet) ? 1U : 0U,
                              current);
      ipv4SettingsChanged = true;
    }
    if (ipv4SettingsChanged) {
      recordEventWithSnapshot(WifiTraceEventType::NetifIpv4SettingsChanged,
                              0,
                              current.ipv4Valid ? 1U : 0U,
                              current);
    }
  }
  previousSnapshot_ = current;
  previousSnapshotValid_ = true;

  const bool inconsistent = current.associated && !current.ipv4Valid;

  if (inconsistent) {
    validIpv4SinceMs_ = 0;
    if (!invalidIpv4Observed_) {
      invalidIpv4Observed_ = true;
      invalidIpv4SinceMs_ = nowMs;
      recordEventWithSnapshot(WifiTraceEventType::Ipv4BecameInvalid,
                              0,
                              0,
                              current);
    }
    if (!invalidIpv4NotificationPending_ &&
        (nowMs - invalidIpv4SinceMs_) >=
            AppConfig::kWifiInvalidIpv4ConfirmationMs &&
        nowMs >= nextInvalidIpv4RecoveryAllowedMs_) {
      invalidIpv4NotificationPending_ = true;
    }
    return;
  }

  if (invalidIpv4Observed_ && current.ipv4Valid) {
    recordEventWithSnapshot(WifiTraceEventType::Ipv4Recovered,
                            0,
                            0,
                            current);
  }
  invalidIpv4SinceMs_ = 0;
  invalidIpv4Observed_ = false;
  invalidIpv4NotificationPending_ = false;

  if (current.ipv4Valid) {
    if (validIpv4SinceMs_ == 0U) {
      validIpv4SinceMs_ = nowMs;
    } else if (invalidIpv4RecoverySeriesCount_ > 0U &&
               (nowMs - validIpv4SinceMs_) >=
                   AppConfig::kWifiRecoverySeriesResetMs) {
      invalidIpv4RecoverySeriesCount_ = 0;
      nextInvalidIpv4RecoveryAllowedMs_ = 0;
      Serial.println(F("[INFO] WIFI_RECOVERY | series_reset_after_stable_ipv4"));
    }
  } else {
    validIpv4SinceMs_ = 0;
  }
}

void WifiManager::flushNewEvents() {
  const uint32_t firstAvailable =
      nextTraceSequence_ > kTraceCapacity
          ? nextTraceSequence_ - kTraceCapacity
          : 1U;
  if (nextPrintSequence_ < firstAvailable) {
    overwrittenEvents_ += firstAvailable - nextPrintSequence_;
    nextPrintSequence_ = firstAvailable;
  }

  while (nextPrintSequence_ < nextTraceSequence_) {
    const WifiTraceEvent* event = traceBySequence(nextPrintSequence_);
    ++nextPrintSequence_;
    if (event == nullptr) {
      continue;
    }
    if (!AppConfig::kPrintDetailedWifiEventsLive &&
        !importantLiveEvent(event->type)) {
      continue;
    }
    if (event->type == WifiTraceEventType::ApUnavailableSummary ||
        event->type == WifiTraceEventType::WifiRecoveredSummary) {
      const uint32_t attempts = event->reason > 0
                                    ? static_cast<uint32_t>(event->reason)
                                    : 0U;
      Serial.print(F("[INFO] WIFI_OUTAGE | state="));
      Serial.print(event->type == WifiTraceEventType::WifiRecoveredSummary
                       ? F("recovered")
                       : F("still_offline"));
      Serial.print(F(" | duration_s="));
      Serial.print(event->detail);
      Serial.print(F(" | no_ap_attempts="));
      Serial.print(attempts);
      Serial.print(F(" | suppressed="));
      Serial.print(attempts > 0U ? attempts - 1U : 0U);
      if (event->type == WifiTraceEventType::WifiRecoveredSummary) {
        Serial.print(F(" | ip="));
        Serial.print(IPAddress(event->localIp));
      }
      Serial.println();
      continue;
    }
    Serial.print(F("[INFO] WIFI_EVENT | seq="));
    Serial.print(event->sequence);
    Serial.print(F(" | uptime_ms="));
    Serial.print(event->uptimeMs);
    Serial.print(F(" | event="));
    Serial.print(eventName(event->type));
    Serial.print(F(" | reason="));
    Serial.print(event->reason);
    if (event->type == WifiTraceEventType::StationDisconnected) {
      Serial.print(F("("));
      Serial.print(disconnectReasonName(event->reason));
      Serial.print(F(")"));
    }
    Serial.print(F(" | detail="));
    Serial.print(event->detail);
    Serial.print(F(" | arduino="));
    Serial.print(event->arduinoStatus);
    Serial.print(F(" | sdk="));
    Serial.print(event->sdkStationStatus);
    Serial.print(F(" | dhcp="));
    Serial.print(event->sdkDhcpStatus);
    Serial.print(F(" | ip="));
    Serial.print(IPAddress(event->localIp));
    Serial.print(F(" | gateway="));
    Serial.print(IPAddress(event->gateway));
    Serial.print(F(" | rssi="));
    Serial.println(event->rssi);
  }
}

void WifiManager::recordCallbackEvent(WifiTraceEventType type,
                                      int16_t reason,
                                      uint16_t detail,
                                      const IPAddress& localIp,
                                      const IPAddress& gateway,
                                      const IPAddress& subnet) {
  // Wi-Fi callbacks must remain small: capture the event and SDK-level state,
  // then let tick() poll lwIP/DHCP and print outside the callback.
  WifiTraceEvent event{};
  event.sequence = nextTraceSequence_++;
  event.uptimeMs = millis();
  event.type = type;
  event.reason = reason;
  event.detail = detail;
  event.arduinoStatus = static_cast<uint8_t>(WiFi.status());
  event.sdkStationStatus =
      static_cast<uint8_t>(wifi_station_get_connect_status());
  event.sdkDhcpStatus =
      static_cast<uint8_t>(wifi_station_dhcpc_status());
  event.localIp = ipToRaw(localIp);
  event.gateway = ipToRaw(gateway);
  event.subnet = ipToRaw(subnet);
  trace_[(event.sequence - 1U) % kTraceCapacity] = event;
}

void WifiManager::recordEvent(WifiTraceEventType type,
                              int16_t reason,
                              uint16_t detail) {
  recordEventWithSnapshot(type, reason, detail, snapshot());
}

void WifiManager::recordEventWithSnapshot(
    WifiTraceEventType type,
    int16_t reason,
    uint16_t detail,
    const WifiRuntimeSnapshot& current) {
  WifiTraceEvent event{};
  event.sequence = nextTraceSequence_++;
  event.uptimeMs = millis();
  event.type = type;
  event.reason = reason;
  event.detail = detail;
  event.arduinoStatus = static_cast<uint8_t>(current.arduinoStatus);
  event.sdkStationStatus = current.sdkStationStatus;
  event.sdkDhcpStatus = current.sdkDhcpStatus;
  event.netifFlags =
      (current.netifUp ? 0x01U : 0U) |
      (current.netifLinkUp ? 0x02U : 0U) |
      (current.dhcpSuppliedAddress ? 0x04U : 0U);
  event.rssi = static_cast<int16_t>(current.rssi);
  event.localIp = ipToRaw(current.localIp);
  event.gateway = ipToRaw(current.gateway);
  event.subnet = ipToRaw(current.subnet);
  trace_[(event.sequence - 1U) % kTraceCapacity] = event;
}

const WifiTraceEvent* WifiManager::traceBySequence(uint32_t sequence) const {
  if (sequence == 0U || sequence >= nextTraceSequence_) {
    return nullptr;
  }
  const WifiTraceEvent& candidate =
      trace_[(sequence - 1U) % kTraceCapacity];
  return candidate.sequence == sequence ? &candidate : nullptr;
}

const WifiTraceEvent* WifiManager::latestRelevantFaultEvent() const {
  if (nextTraceSequence_ <= 1U) {
    return nullptr;
  }

  const uint32_t firstAvailable =
      nextTraceSequence_ > kTraceCapacity
          ? nextTraceSequence_ - kTraceCapacity
          : 1U;
  const uint32_t now = millis();
  const WifiTraceEvent* latestDhcpTimeout = nullptr;
  const WifiTraceEvent* latestDisconnect = nullptr;
  const WifiTraceEvent* latestAnyDisconnect = nullptr;
  const WifiTraceEvent* latestIpv4Change = nullptr;

  for (uint32_t sequence = nextTraceSequence_ - 1U;
       sequence >= firstAvailable;
       --sequence) {
    const WifiTraceEvent* event = traceBySequence(sequence);
    if (event != nullptr) {
      if ((now - event->uptimeMs) > AppConfig::kWifiFaultHistoryWindowMs) {
        break;
      }
      if (event->type == WifiTraceEventType::DhcpTimeout &&
          latestDhcpTimeout == nullptr) {
        latestDhcpTimeout = event;
      } else if (event->type == WifiTraceEventType::StationDisconnected) {
        if (latestAnyDisconnect == nullptr) {
          latestAnyDisconnect = event;
        }
        if (event->detail == 0U && latestDisconnect == nullptr) {
          // Prefer an unsolicited AP/association disconnect over a secondary
          // event caused by a connection attempt or intentional recycle.
          latestDisconnect = event;
        }
      } else if ((event->type ==
                      WifiTraceEventType::NetifIpv4AddressChanged ||
                  event->type ==
                      WifiTraceEventType::NetifIpv4GatewayChanged ||
                  event->type ==
                      WifiTraceEventType::NetifIpv4SettingsChanged) &&
                 latestIpv4Change == nullptr) {
        latestIpv4Change = event;
      }
    }
    if (sequence == firstAvailable) {
      break;
    }
  }

  if (latestDhcpTimeout != nullptr) {
    return latestDhcpTimeout;
  }
  if (latestDisconnect != nullptr) {
    return latestDisconnect;
  }
  if (latestAnyDisconnect != nullptr) {
    return latestAnyDisconnect;
  }
  return latestIpv4Change;
}

struct netif* WifiManager::stationNetif() const {
  struct netif* interface = nullptr;
  NETIF_FOREACH(interface) {
    if (interface->name[0] == 's' && interface->name[1] == 't') {
      return interface;
    }
  }
  return netif_default;
}

const __FlashStringHelper* WifiManager::eventName(WifiTraceEventType type) {
  switch (type) {
    case WifiTraceEventType::Begin:
      return F("BEGIN");
    case WifiTraceEventType::ConnectionAttempt:
      return F("CONNECTION_ATTEMPT");
    case WifiTraceEventType::ForcedRecycle:
      return F("FORCED_RECYCLE");
    case WifiTraceEventType::StationConnected:
      return F("STATION_CONNECTED");
    case WifiTraceEventType::StationDisconnected:
      return F("STATION_DISCONNECTED");
    case WifiTraceEventType::GotIp:
      return F("GOT_IP");
    case WifiTraceEventType::DhcpTimeout:
      return F("DHCP_TIMEOUT");
    case WifiTraceEventType::ModeChanged:
      return F("MODE_CHANGED");
    case WifiTraceEventType::AuthModeChanged:
      return F("AUTH_MODE_CHANGED");
    case WifiTraceEventType::NetifLinkChanged:
      return F("NETIF_LINK_CHANGED");
    case WifiTraceEventType::NetifStatusChanged:
      return F("NETIF_STATUS_CHANGED");
    case WifiTraceEventType::NetifIpv4AddressChanged:
      return F("NETIF_IPV4_ADDRESS_CHANGED");
    case WifiTraceEventType::NetifIpv4GatewayChanged:
      return F("NETIF_IPV4_GATEWAY_CHANGED");
    case WifiTraceEventType::NetifIpv4NetmaskChanged:
      return F("NETIF_IPV4_NETMASK_CHANGED");
    case WifiTraceEventType::NetifIpv4SettingsChanged:
      return F("NETIF_IPV4_SETTINGS_CHANGED");
    case WifiTraceEventType::Ipv4BecameInvalid:
      return F("IPV4_BECAME_INVALID");
    case WifiTraceEventType::Ipv4Recovered:
      return F("IPV4_RECOVERED");
    case WifiTraceEventType::ApUnavailableSummary:
      return F("NO_AP_FOUND_SUMMARY");
    case WifiTraceEventType::WifiRecoveredSummary:
      return F("WIFI_RECOVERED_SUMMARY");
  }
  return F("UNKNOWN");
}

const __FlashStringHelper* WifiManager::disconnectReasonName(int reason) {
  switch (reason) {
    case WIFI_DISCONNECT_REASON_UNSPECIFIED:
      return F("UNSPECIFIED");
    case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:
      return F("AUTH_EXPIRE");
    case WIFI_DISCONNECT_REASON_AUTH_LEAVE:
      return F("AUTH_LEAVE");
    case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE:
      return F("ASSOC_EXPIRE");
    case WIFI_DISCONNECT_REASON_ASSOC_TOOMANY:
      return F("ASSOC_TOO_MANY");
    case WIFI_DISCONNECT_REASON_NOT_AUTHED:
      return F("NOT_AUTHED");
    case WIFI_DISCONNECT_REASON_NOT_ASSOCED:
      return F("NOT_ASSOCED");
    case WIFI_DISCONNECT_REASON_ASSOC_LEAVE:
      return F("ASSOC_LEAVE");
    case WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED:
      return F("ASSOC_NOT_AUTHED");
    case WIFI_DISCONNECT_REASON_DISASSOC_PWRCAP_BAD:
      return F("DISASSOC_PWRCAP_BAD");
    case WIFI_DISCONNECT_REASON_DISASSOC_SUPCHAN_BAD:
      return F("DISASSOC_SUPCHAN_BAD");
    case WIFI_DISCONNECT_REASON_IE_INVALID:
      return F("IE_INVALID");
    case WIFI_DISCONNECT_REASON_MIC_FAILURE:
      return F("MIC_FAILURE");
    case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return F("4WAY_HANDSHAKE_TIMEOUT");
    case WIFI_DISCONNECT_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return F("GROUP_KEY_UPDATE_TIMEOUT");
    case WIFI_DISCONNECT_REASON_IE_IN_4WAY_DIFFERS:
      return F("IE_IN_4WAY_DIFFERS");
    case WIFI_DISCONNECT_REASON_GROUP_CIPHER_INVALID:
      return F("GROUP_CIPHER_INVALID");
    case WIFI_DISCONNECT_REASON_PAIRWISE_CIPHER_INVALID:
      return F("PAIRWISE_CIPHER_INVALID");
    case WIFI_DISCONNECT_REASON_AKMP_INVALID:
      return F("AKMP_INVALID");
    case WIFI_DISCONNECT_REASON_UNSUPP_RSN_IE_VERSION:
      return F("UNSUPP_RSN_IE_VERSION");
    case WIFI_DISCONNECT_REASON_INVALID_RSN_IE_CAP:
      return F("INVALID_RSN_IE_CAP");
    case WIFI_DISCONNECT_REASON_802_1X_AUTH_FAILED:
      return F("802_1X_AUTH_FAILED");
    case WIFI_DISCONNECT_REASON_CIPHER_SUITE_REJECTED:
      return F("CIPHER_SUITE_REJECTED");
    case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:
      return F("BEACON_TIMEOUT");
    case WIFI_DISCONNECT_REASON_NO_AP_FOUND:
      return F("NO_AP_FOUND");
    case WIFI_DISCONNECT_REASON_AUTH_FAIL:
      return F("AUTH_FAIL");
    case WIFI_DISCONNECT_REASON_ASSOC_FAIL:
      return F("ASSOC_FAIL");
    case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:
      return F("HANDSHAKE_TIMEOUT");
    default:
      return F("OTHER");
  }
}

const __FlashStringHelper* WifiManager::stationStatusName(uint8_t status) {
  switch (status) {
    case STATION_IDLE:
      return F("IDLE");
    case STATION_CONNECTING:
      return F("CONNECTING");
    case STATION_WRONG_PASSWORD:
      return F("WRONG_PASSWORD");
    case STATION_NO_AP_FOUND:
      return F("NO_AP_FOUND");
    case STATION_CONNECT_FAIL:
      return F("CONNECT_FAIL");
    case STATION_GOT_IP:
      return F("GOT_IP");
    default:
      return F("UNKNOWN");
  }
}

const __FlashStringHelper* WifiManager::dhcpStatusName(uint8_t status) {
  switch (status) {
    case DHCP_STOPPED:
      return F("STOPPED");
    case DHCP_STARTED:
      return F("STARTED");
    default:
      return F("UNKNOWN");
  }
}
