#pragma once

#include <Arduino.h>
#include <CertStoreBearSSL.h>
#include <WiFiClientSecureBearSSL.h>

class TlsManager {
 public:
  bool begin(bool filesystemReady);
  bool configureNtfy(BearSSL::WiFiClientSecure& client,
                     bool timeValid,
                     time_t currentEpoch,
                     uint32_t timeoutMs);

  bool ready() const;
  int certificateCount() const;

 private:
  BearSSL::CertStore certificateStore_{};
  bool ready_ = false;
  int certificateCount_ = 0;
};
