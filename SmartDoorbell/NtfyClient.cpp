#include "NtfyClient.h"

#include <ESP8266WiFi.h>
#include <cstring>

#include "AppConfig.h"
#include "Secrets.h"
#include "TcpDiagnostics.h"

NtfyClient::~NtfyClient() {
  close();
}

bool NtfyClient::begin() {
  endpointReady_ = validateEndpoint(String(Secrets::kNtfyBaseUrl));
  return endpointReady_;
}

void NtfyClient::setEnvironment(bool wifiConnected,
                                bool timeValid,
                                time_t currentEpoch) {
  wifiConnected_ = wifiConnected;
  timeValid_ = timeValid;
  currentEpoch_ = currentEpoch;
}

void NtfyClient::tick(uint64_t nowMs, bool wifiConnected) {
  if (!httpReady_) {
    return;
  }

  if (!wifiConnected) {
    resetTransport(F("wifi_lost"), true);
    return;
  }

  // lastActivityMs_ comes from 32-bit millis(), while nowMs is extended uptime.
  // Subtract in the same clock domain, including when millis() wraps to zero.
  const uint32_t idleMs = static_cast<uint32_t>(nowMs) - lastActivityMs_;
  if (idleMs >= AppConfig::kNtfyKeepAliveIdleMs) {
    // The response has already been consumed. Abort the idle socket so the
    // ESP8266 does not become the active closer and accumulate TIME_WAIT PCBs.
    resetTransport(F("keepalive_idle"), true,
                   AppConfig::kVerboseOperationalLogs);
  }
}

void NtfyClient::close() {
  resetTransport(F("explicit_close"), true,
                 AppConfig::kVerboseOperationalLogs);
}

const String& NtfyClient::host() const {
  return host_;
}

uint16_t NtfyClient::port() const {
  return port_;
}

NtfyPublishResult NtfyClient::publish(const DeliveryAttempt& attempt,
                                      const String& title,
                                      const String& message,
                                      InputPump inputPump) {
  const uint32_t startedMs = millis();
  NtfyPublishResult result{};
  result.tcpTimeWaitBefore = TcpDiagnostics::snapshot().timeWaitPcbs;

  if (!endpointReady_) {
    result.code = NtfyResultCode::InvalidEndpoint;
    result.error = F("invalid ntfy HTTPS endpoint");
    return result;
  }
  if (!validateEnvironment(result)) {
    result.totalMs = millis() - startedMs;
    return result;
  }

  const String path = publishPath(attempt.sequenceId);
  result = postOnce(attempt, title, message, path, inputPump);

  // A keep-alive socket can be closed by the remote endpoint between updates.
  // Retry exactly once with a fully rebuilt client only when the failed request
  // had attempted to reuse an already-open connection.
  if (!result.success && result.connectionReused &&
      isReconnectableFailure(result.code)) {
    const String firstError = result.error;
    const uint32_t firstDuration = result.totalMs;
    const uint16_t timeWaitBefore = result.tcpTimeWaitBefore;

    result.reconnectAttempted = true;
    resetTransport(F("stale_keepalive"), true);
    cooperativeDelay(AppConfig::kNtfyFreshReconnectDelayMs, inputPump);

    NtfyPublishResult retry =
        postOnce(attempt, title, message, path, inputPump);
    const String freshError = retry.error;
    retry.reconnectAttempted = true;
    retry.reconnectSucceeded = retry.success;
    retry.tcpTimeWaitBefore = timeWaitBefore;
    retry.totalMs += firstDuration + AppConfig::kNtfyFreshReconnectDelayMs;
    if (!retry.success) {
      retry.error = F("keepalive=[");
      retry.error += firstError;
      retry.error += F("]; fresh=[");
      retry.error += freshError;
      retry.error += F("]");
    }
    result = retry;
  }

  if (result.success || result.httpStatus > 0) {
    consecutiveConnectFailures_ = 0;
  } else if (isReconnectableFailure(result.code)) {
    if (consecutiveConnectFailures_ < UINT8_MAX) {
      ++consecutiveConnectFailures_;
    }
    if (consecutiveConnectFailures_ >=
        AppConfig::kWifiRecycleAfterConnectFailures) {
      result.wifiRecycleRequested = true;
      consecutiveConnectFailures_ = 0;
      resetTransport(F("wifi_recycle_requested"), true);
    }
  }

  result.transportGeneration = transportGeneration_;
  result.tcpTimeWaitAfter = TcpDiagnostics::snapshot().timeWaitPcbs;
  result.totalMs = millis() - startedMs;
  return result;
}

bool NtfyClient::validateEndpoint(const String& baseUrl) {
  if (!baseUrl.startsWith(F("https://"))) {
    return false;
  }

  String authorityAndPath = baseUrl.substring(8);
  const int slash = authorityAndPath.indexOf('/');
  String authority = slash >= 0
                         ? authorityAndPath.substring(0, slash)
                         : authorityAndPath;
  basePath_ = slash >= 0 ? authorityAndPath.substring(slash) : String();

  while (basePath_.endsWith(F("/"))) {
    basePath_.remove(basePath_.length() - 1U);
  }

  port_ = 443;
  const int colon = authority.lastIndexOf(':');
  if (colon > 0) {
    const long parsedPort = authority.substring(colon + 1).toInt();
    if (parsedPort < 1 || parsedPort > 65535) {
      return false;
    }
    port_ = static_cast<uint16_t>(parsedPort);
    authority.remove(colon);
  }

  host_ = authority;
  host_.trim();
  return host_.length() > 0U;
}

bool NtfyClient::validateEnvironment(NtfyPublishResult& result) const {
  if (!wifiConnected_ || WiFi.status() != WL_CONNECTED) {
    result.code = NtfyResultCode::WifiUnavailable;
    result.error = F("Wi-Fi unavailable");
    return false;
  }
  if (!timeValid_) {
    result.code = NtfyResultCode::TimeUnavailable;
    result.error = F("valid time unavailable");
    return false;
  }
  if (!tlsManager_.ready()) {
    result.code = NtfyResultCode::TlsUnavailable;
    result.error = F("TLS certificate store unavailable");
    return false;
  }
  return true;
}

String NtfyClient::publishPath(const char* sequenceId) const {
  String path;
  path.reserve(basePath_.length() + strlen(Secrets::kNtfyTopic) + 80U);
  if (basePath_.length() > 0U) {
    path += basePath_;
  }
  path += '/';
  path += Secrets::kNtfyTopic;
  path += '/';
  path += sequenceId;
  return path;
}

String NtfyClient::priority(const DeliveryAttempt& attempt) const {
  const bool audible = attempt.firstSuccessfulPublish && attempt.urgent;
  if (!audible) {
    return F("low");
  }
  return attempt.stage == NotificationStage::Insistence
             ? String(F("max"))
             : String(F("high"));
}

bool NtfyClient::ensureTransport(const String& path,
                                 NtfyPublishResult& result) {
  if (httpReady_) {
    if (http_.setURL(path)) {
      return true;
    }
    resetTransport(F("set_url_failed"), true);
  }

  if (!tlsManager_.configureNtfy(secureClient_,
                                 timeValid_,
                                 currentEpoch_,
                                 AppConfig::kNtfyHttpTimeoutMs)) {
    result.code = NtfyResultCode::TlsUnavailable;
    result.error = F("TLS configuration failed");
    return false;
  }

  if (!http_.begin(secureClient_, host_, port_, path, true)) {
    result.code = NtfyResultCode::InvalidEndpoint;
    result.error = F("HTTPClient begin failed");
    return false;
  }

  configureHttpClient();
  httpReady_ = true;
  ++transportGeneration_;
  return true;
}

void NtfyClient::configureHttpClient() {
  http_.setReuse(true);
  http_.setTimeout(AppConfig::kNtfyHttpTimeoutMs);
  http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http_.setUserAgent(String(F("COMMAX-ESP8266-Doorbell/")) +
                     AppConfig::kFirmwareVersion);
  const char* responseHeaders[] = {"Connection", "Keep-Alive"};
  http_.collectHeaders(responseHeaders, 2);
}

void NtfyClient::addRequestHeaders(const DeliveryAttempt& attempt,
                                   const String& title) {
  http_.addHeader(F("Content-Type"), F("text/plain; charset=utf-8"));
  http_.addHeader(F("Title"), title);
  http_.addHeader(F("Priority"), priority(attempt));
  http_.addHeader(F("Tags"), F("bell"));
  if (Secrets::kNtfyClickUrl[0] != '\0') {
    http_.addHeader(F("Click"), Secrets::kNtfyClickUrl);
  }
  if (Secrets::kNtfyAction[0] != '\0') {
    http_.addHeader(F("Actions"), Secrets::kNtfyAction);
  }
}

NtfyPublishResult NtfyClient::postOnce(const DeliveryAttempt& attempt,
                                       const String& title,
                                       const String& message,
                                       const String& path,
                                       InputPump inputPump) {
  NtfyPublishResult result{};
  const uint32_t startedMs = millis();
  result.tcpTimeWaitBefore = TcpDiagnostics::snapshot().timeWaitPcbs;

  if (!ensureTransport(path, result)) {
    result.totalMs = millis() - startedMs;
    result.tcpTimeWaitAfter = TcpDiagnostics::snapshot().timeWaitPcbs;
    return result;
  }

  result.connectionReused = http_.connected();
  addRequestHeaders(attempt, title);

  if (inputPump != nullptr) {
    inputPump();
  }
  yield();

  Serial.print(F("[INFO] NTFY_HTTPCLIENT | post_begin | sequence="));
  Serial.print(attempt.sequenceId);
  Serial.print(F(" | bytes="));
  Serial.print(message.length());
  Serial.print(F(" | connection="));
  Serial.print(result.connectionReused ? F("reused") : F("new"));
  Serial.print(F(" | generation="));
  Serial.print(transportGeneration_);
  Serial.print(F(" | tcp_time_wait="));
  Serial.println(result.tcpTimeWaitBefore);

  const int httpCode = http_.POST(
      reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
  result.httpStatus = httpCode > 0 ? httpCode : 0;
  result.httpClientError = httpCode < 0 ? httpCode : 0;

  if (inputPump != nullptr) {
    inputPump();
  }
  yield();

  if (httpCode > 0) {
    const String responseBody = http_.getString();
    result.responseBytes = responseBody.length();
  }

  result.keepAliveRetained = http_.connected();
  if (result.keepAliveRetained) {
    WiFiClient* stream = http_.getStreamPtr();
    if (stream != nullptr) {
      result.localPort = stream->localPort();
    }
  }

  if (httpCode == HTTP_CODE_OK) {
    result.success = true;
    result.code = NtfyResultCode::Success;
  } else if (httpCode > 0) {
    result.code = NtfyResultCode::HttpError;
    result.error = F("HTTP status ");
    result.error += String(httpCode);
  } else {
    result.sslError = captureSslError(result.error);
    result.code = classifyHttpClientError(httpCode, result.sslError);
    if (result.error.length() == 0U) {
      result.error = HTTPClient::errorToString(httpCode);
    } else {
      result.error += F("; httpclient=");
      result.error += HTTPClient::errorToString(httpCode);
    }
    resetTransport(F("request_failed"), true);
  }

  lastActivityMs_ = millis();
  result.transportGeneration = transportGeneration_;
  result.tcpTimeWaitAfter = TcpDiagnostics::snapshot().timeWaitPcbs;
  result.totalMs = millis() - startedMs;
  return result;
}

bool NtfyClient::isReconnectableFailure(NtfyResultCode code) const {
  return code == NtfyResultCode::ConnectFailed ||
         code == NtfyResultCode::TlsHandshakeFailed ||
         code == NtfyResultCode::WriteFailed ||
         code == NtfyResultCode::ResponseTimeout;
}

void NtfyClient::resetTransport(const __FlashStringHelper* reason,
                                bool abortSocket,
                                bool logClose) {
  if (!httpReady_) {
    return;
  }

  const uint16_t before =
      logClose ? TcpDiagnostics::snapshot().timeWaitPcbs : 0U;
  WiFiClient* stream = http_.getStreamPtr();
  const uint16_t localPort =
      logClose && stream != nullptr ? stream->localPort() : 0U;
  if (stream != nullptr && abortSocket) {
    stream->abort();
  }

  http_.setReuse(false);
  http_.end();
  secureClient_.stop();
  httpReady_ = false;
  lastActivityMs_ = 0;

  if (logClose) {
    Serial.print(F("[INFO] NTFY_KEEPALIVE | closed | reason="));
    Serial.print(reason);
    Serial.print(F(" | abort="));
    Serial.print(abortSocket ? F("yes") : F("no"));
    Serial.print(F(" | local_port="));
    Serial.print(localPort);
    Serial.print(F(" | tcp_time_wait_before="));
    Serial.print(before);
    Serial.print(F(" | tcp_time_wait_after="));
    Serial.println(TcpDiagnostics::snapshot().timeWaitPcbs);
  }
}

void NtfyClient::cooperativeDelay(uint32_t durationMs,
                                  InputPump inputPump) const {
  const uint32_t started = millis();
  while ((millis() - started) < durationMs) {
    if (inputPump != nullptr) {
      inputPump();
    }
    delay(10);
    yield();
  }
}

NtfyResultCode NtfyClient::classifyHttpClientError(int errorCode,
                                                   int sslError) const {
  switch (errorCode) {
    case HTTPC_ERROR_CONNECTION_FAILED:
      return sslError == 0 ? NtfyResultCode::ConnectFailed
                           : NtfyResultCode::TlsHandshakeFailed;
    case HTTPC_ERROR_SEND_HEADER_FAILED:
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
    case HTTPC_ERROR_STREAM_WRITE:
      return NtfyResultCode::WriteFailed;
    case HTTPC_ERROR_READ_TIMEOUT:
      return NtfyResultCode::ResponseTimeout;
    case HTTPC_ERROR_NOT_CONNECTED:
    case HTTPC_ERROR_CONNECTION_LOST:
    case HTTPC_ERROR_NO_STREAM:
    case HTTPC_ERROR_NO_HTTP_SERVER:
      return NtfyResultCode::ConnectFailed;
    case HTTPC_ERROR_TOO_LESS_RAM:
    case HTTPC_ERROR_ENCODING:
    default:
      return NtfyResultCode::InvalidResponse;
  }
}

int NtfyClient::captureSslError(String& destination) const {
  if (!httpReady_) {
    return 0;
  }

  WiFiClient* stream = const_cast<HTTPClient&>(http_).getStreamPtr();
  if (stream == nullptr) {
    return 0;
  }

  auto* secure = static_cast<BearSSL::WiFiClientSecure*>(stream);
  char detail[128]{};
  const int code = secure->getLastSSLError(detail, sizeof(detail));
  if (code == 0) {
    return 0;
  }

  destination = F("TLS error ");
  destination += String(code);
  if (detail[0] != '\0') {
    destination += F(" ");
    destination += detail;
  }
  return code;
}
