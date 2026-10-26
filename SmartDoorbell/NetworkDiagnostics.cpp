#include "NetworkDiagnostics.h"

#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "AppConfig.h"
#include "TcpDiagnostics.h"

namespace {

const __FlashStringHelper* yesNo(bool value) {
  return value ? F("yes") : F("no");
}

}  // namespace

bool NetworkDiagnostics::setLocalTarget(const String& ipv4Text,
                                        uint16_t port) {
  IPAddress parsed{};
  if (port == 0U || !parsed.fromString(ipv4Text)) {
    return false;
  }
  localTargetIp_ = parsed;
  localTargetPort_ = port;
  localTargetConfigured_ = true;
  return true;
}

void NetworkDiagnostics::clearLocalTarget() {
  localTargetIp_ = IPAddress();
  localTargetPort_ = 0;
  localTargetConfigured_ = false;
}

bool NetworkDiagnostics::localTargetConfigured() const {
  return localTargetConfigured_;
}

void NetworkDiagnostics::printConfiguration() const {
  Serial.print(F("[INFO] NET_DIAG_CONFIG | local_target="));
  if (!localTargetConfigured_) {
    Serial.println(F("disabled | command=diagpc_<IPv4>_<port>"));
    return;
  }
  Serial.print(localTargetIp_);
  Serial.print(':');
  Serial.println(localTargetPort_);
}

bool NetworkDiagnostics::automaticDue(uint64_t nowMs) const {
  return lastAutomaticRunMs_ == 0U ||
         (nowMs - lastAutomaticRunMs_) >=
             AppConfig::kNetworkDiagnosticsAutomaticCooldownMs;
}

void NetworkDiagnostics::run(uint64_t nowMs,
                             const String& ntfyHost,
                             uint16_t ntfyPort,
                             bool timeValid,
                             time_t currentEpoch,
                             InputPump inputPump,
                             bool automatic,
                             const __FlashStringHelper* triggerName) {
  if (automatic) {
    lastAutomaticRunMs_ = nowMs;
  }

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  const TcpDiagnosticSnapshot tcpBefore = TcpDiagnostics::snapshot();

  Serial.print(F("[WARN] NET_DIAG | begin | trigger="));
  Serial.print(triggerName != nullptr
                   ? triggerName
                   : (automatic ? F("automatic_failure") : F("manual")));
  Serial.print(F(" | wifi="));
  Serial.print(wifiConnected ? F("connected") : F("disconnected"));
  Serial.print(F(" | status="));
  Serial.print(static_cast<int>(WiFi.status()));
  Serial.print(F(" | rssi="));
  Serial.print(wifiConnected ? WiFi.RSSI() : 0);
  Serial.print(F(" | local_ip="));
  Serial.print(WiFi.localIP());
  Serial.print(F(" | gateway="));
  Serial.print(WiFi.gatewayIP());
  Serial.print(F(" | dns="));
  Serial.print(WiFi.dnsIP());
  Serial.print(F(" | active_pcbs="));
  Serial.print(tcpBefore.activePcbs);
  Serial.print(F(" | time_wait_pcbs="));
  Serial.println(tcpBefore.timeWaitPcbs);

  if (!wifiConnected) {
    printClassification(false, false, false, false, false,
                        false, false, false, false);
    Serial.println(F("[WARN] NET_DIAG | end"));
    return;
  }

  const NetworkProbeResult ntfyDns = resolveHost(ntfyHost, inputPump);
  printProbe(F("dns_ntfy"), ntfyDns);

  const String alternateHost(AppConfig::kNetworkDiagnosticsAlternateHost);
  const NetworkProbeResult alternateDns =
      resolveHost(alternateHost, inputPump);
  printProbe(F("dns_alternate"), alternateDns);

  NetworkProbeResult localTcp{};
  if (localTargetConfigured_) {
    localTcp = probeTcp(localTargetIp_, localTargetPort_, inputPump, true);
    printProbe(F("tcp_local_pc"), localTcp, localTargetPort_);
  } else {
    Serial.println(F("[INFO] NET_DIAG_PROBE | name=tcp_local_pc | result=SKIPPED | reason=diagpc_not_configured"));
  }

  NetworkProbeResult ntfyTcp{};
  if (ntfyDns.success) {
    ntfyTcp = probeTcp(ntfyDns.address, ntfyPort, inputPump, false);
  }
  printProbe(F("tcp_ntfy"), ntfyTcp, ntfyPort);

  NetworkProbeResult alternateTcp{};
  if (alternateDns.success) {
    alternateTcp = probeTcp(alternateDns.address,
                            AppConfig::kNetworkDiagnosticsAlternatePort,
                            inputPump,
                            false);
  }
  printProbe(F("tcp_alternate"), alternateTcp,
             AppConfig::kNetworkDiagnosticsAlternatePort);

  NetworkProbeResult ntfyTls{};
  if (ntfyTcp.success) {
    ntfyTls = probeTls(ntfyHost, ntfyPort, timeValid,
                       currentEpoch, inputPump);
  }
  printProbe(F("tls_ntfy"), ntfyTls, ntfyPort);

  NetworkProbeResult alternateTls{};
  if (alternateTcp.success) {
    alternateTls = probeTls(alternateHost,
                            AppConfig::kNetworkDiagnosticsAlternatePort,
                            timeValid,
                            currentEpoch,
                            inputPump);
  }
  printProbe(F("tls_alternate"), alternateTls,
             AppConfig::kNetworkDiagnosticsAlternatePort);

  const TcpDiagnosticSnapshot tcpAfter = TcpDiagnostics::snapshot();
  Serial.print(F("[INFO] NET_DIAG_TCP | active_pcbs="));
  Serial.print(tcpBefore.activePcbs);
  Serial.print(F("->"));
  Serial.print(tcpAfter.activePcbs);
  Serial.print(F(" | time_wait_pcbs="));
  Serial.print(tcpBefore.timeWaitPcbs);
  Serial.print(F("->"));
  Serial.print(tcpAfter.timeWaitPcbs);
  Serial.print(F(" | bound_pcbs="));
  Serial.print(tcpAfter.boundPcbs);
  Serial.print(F(" | listen_pcbs="));
  Serial.println(tcpAfter.listenPcbs);

  printClassification(true,
                      ntfyDns.success,
                      alternateDns.success,
                      localTcp.attempted,
                      localTcp.success,
                      ntfyTcp.success,
                      alternateTcp.success,
                      ntfyTls.success,
                      alternateTls.success);
  Serial.println(F("[WARN] NET_DIAG | end"));
}

NetworkProbeResult NetworkDiagnostics::resolveHost(
    const String& host, InputPump inputPump) const {
  NetworkProbeResult result{};
  result.attempted = true;
  const uint32_t startedMs = millis();
  if (inputPump != nullptr) {
    inputPump();
  }
  result.success = WiFi.hostByName(host.c_str(), result.address) == 1;
  result.durationMs = millis() - startedMs;
  yield();
  return result;
}

NetworkProbeResult NetworkDiagnostics::probeTcp(
    const IPAddress& address,
    uint16_t port,
    InputPump inputPump,
    bool sendLocalMarker) const {
  NetworkProbeResult result{};
  result.attempted = true;
  result.address = address;

  WiFiClient client;
  client.setNoDelay(true);
  client.setTimeout(AppConfig::kNetworkDiagnosticsConnectTimeoutMs);

  if (inputPump != nullptr) {
    inputPump();
  }
  const uint32_t startedMs = millis();
  result.success = client.connect(address, port);
  result.durationMs = millis() - startedMs;
  if (result.success && sendLocalMarker) {
    client.write(reinterpret_cast<const uint8_t*>("RC17\n"), 5U);
  }
  client.abort();
  cooperativePause(AppConfig::kNetworkDiagnosticsInterProbeDelayMs,
                   inputPump);
  return result;
}

NetworkProbeResult NetworkDiagnostics::probeTls(
    const String& host,
    uint16_t port,
    bool timeValid,
    time_t currentEpoch,
    InputPump inputPump) const {
  NetworkProbeResult result{};
  result.attempted = true;

  BearSSL::WiFiClientSecure client;
  if (!tlsManager_.configureNtfy(client,
                                 timeValid,
                                 currentEpoch,
                                 AppConfig::kNetworkDiagnosticsConnectTimeoutMs)) {
    result.sslError = -1;
    return result;
  }

  if (inputPump != nullptr) {
    inputPump();
  }
  const uint32_t startedMs = millis();
  result.success = client.connect(host.c_str(), port);
  result.durationMs = millis() - startedMs;
  if (!result.success) {
    char detail[96]{};
    result.sslError = client.getLastSSLError(detail, sizeof(detail));
  }
  client.abort();
  cooperativePause(AppConfig::kNetworkDiagnosticsInterProbeDelayMs,
                   inputPump);
  return result;
}

void NetworkDiagnostics::printProbe(
    const __FlashStringHelper* name,
    const NetworkProbeResult& result,
    uint16_t port) const {
  Serial.print(F("[INFO] NET_DIAG_PROBE | name="));
  Serial.print(name);
  Serial.print(F(" | attempted="));
  Serial.print(yesNo(result.attempted));
  Serial.print(F(" | success="));
  Serial.print(yesNo(result.success));
  if (result.address != IPAddress()) {
    Serial.print(F(" | ip="));
    Serial.print(result.address);
  }
  if (port != 0U) {
    Serial.print(F(" | port="));
    Serial.print(port);
  }
  Serial.print(F(" | duration_ms="));
  Serial.print(result.durationMs);
  Serial.print(F(" | ssl="));
  Serial.println(result.sslError);
}

void NetworkDiagnostics::printClassification(
    bool wifiConnected,
    bool ntfyDns,
    bool alternateDns,
    bool localAttempted,
    bool localTcp,
    bool ntfyTcp,
    bool alternateTcp,
    bool ntfyTls,
    bool alternateTls) const {
  const __FlashStringHelper* classification = F("INCONCLUSIVE");
  const __FlashStringHelper* confidence = F("low");

  if (!wifiConnected) {
    classification = F("WIFI_ASSOCIATION_OR_INTERFACE_DOWN");
    confidence = F("high");
  } else if (!ntfyDns && !alternateDns) {
    classification = F("DNS_PATH_FAILURE");
    confidence = F("high");
  } else if (localAttempted && !localTcp && !ntfyTcp && !alternateTcp) {
    classification = F("ESP8266_OR_LOCAL_LAN_TCP_FAILURE");
    confidence = F("high");
  } else if (localAttempted && localTcp && !ntfyTcp && alternateTcp) {
    classification = F("NTFY_ROUTE_OR_ENDPOINT_SPECIFIC");
    confidence = F("high");
  } else if (localAttempted && localTcp && !ntfyTcp && !alternateTcp) {
    classification = F("ROUTER_NAT_ISP_OR_WAN_TCP_FAILURE");
    confidence = F("high");
  } else if (ntfyTcp && !ntfyTls && alternateTls) {
    classification = F("NTFY_TLS_OR_CERTIFICATE_SPECIFIC");
    confidence = F("high");
  } else if (ntfyTcp && alternateTcp && !ntfyTls && !alternateTls) {
    classification = F("BEARSSL_CERTSTORE_OR_TLS_LAYER_FAILURE");
    confidence = F("high");
  } else if (ntfyTls && alternateTls) {
    classification = F("ALL_NETWORK_LAYERS_OK");
    confidence = F("high");
  } else if (ntfyTls) {
    classification = F("NTFY_PATH_OK_ALTERNATE_PATH_FAILED");
    confidence = F("medium");
  } else if (!localAttempted && !ntfyTcp && !alternateTcp) {
    classification = F("PUBLIC_TCP_UNAVAILABLE_LOCAL_TARGET_REQUIRED");
    confidence = F("medium");
  } else if (!ntfyTcp && alternateTcp) {
    classification = F("NTFY_ROUTE_OR_ENDPOINT_SPECIFIC");
    confidence = F("medium");
  }

  Serial.print(F("[WARN] NET_DIAG_RESULT | classification="));
  Serial.print(classification);
  Serial.print(F(" | confidence="));
  Serial.print(confidence);
  Serial.print(F(" | local_target="));
  Serial.println(localAttempted ? F("tested") : F("not_configured"));
}

void NetworkDiagnostics::cooperativePause(
    uint32_t durationMs, InputPump inputPump) const {
  const uint32_t startedMs = millis();
  while ((millis() - startedMs) < durationMs) {
    if (inputPump != nullptr) {
      inputPump();
    }
    delay(10);
    yield();
  }
}
