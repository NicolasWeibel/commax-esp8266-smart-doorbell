#include "TlsManager.h"

#include <LittleFS.h>

#include "AppConfig.h"

bool TlsManager::begin(bool filesystemReady) {
  ready_ = false;
  certificateCount_ = 0;

  if (filesystemReady && LittleFS.exists(AppConfig::kCertArchivePath) &&
      LittleFS.exists(AppConfig::kCertIndexPath)) {
    certificateCount_ = certificateStore_.initCertStore(
        LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    ready_ = certificateCount_ > 0;
  }

  return ready_;
}

bool TlsManager::configureNtfy(BearSSL::WiFiClientSecure& client,
                               bool timeValid,
                               time_t currentEpoch,
                               uint32_t timeoutMs) {
  WiFiClient::setDefaultNoDelay(true);
  client.setSSLVersion(BR_TLS12, BR_TLS12);
  client.setTimeout(timeoutMs);
  client.setBufferSizes(AppConfig::kNtfyTlsReceiveBufferBytes,
                        AppConfig::kNtfyTlsTransmitBufferBytes);

  // RC17 uses BearSSL's default cipher selection. HTTPClient owns a reusable
  // TLS connection; the firmware still does not manage BearSSL Session objects.
  if (!ready_ || !timeValid || currentEpoch <= 0) {
    return false;
  }

  client.setX509Time(currentEpoch);
  client.setCertStore(&certificateStore_);
  return true;
}

bool TlsManager::ready() const {
  return ready_;
}


int TlsManager::certificateCount() const {
  return certificateCount_;
}
