#pragma once

#include <Arduino.h>

#include "AppTypes.h"

class ConnectivityManager {
 public:
  void begin(uint64_t nowMs, bool restoredOffline = false);
  void updateEnvironment(bool wifiConnected,
                         bool timeValid,
                         uint64_t nowMs);
  void recordPublishFailure(uint64_t nowMs);
  void recordPublishSuccess(uint64_t nowMs);
  void beginRecovery(uint64_t nowMs);
  void recordRecoveryFailure(uint64_t nowMs);

  ConnectivityStatus status() const;
  bool consumeBecameUnavailable();
  bool consumeRecovered();

 private:
  void setOffline(OfflineReason reason, uint64_t nowMs);
  void resetFailureSeries();

  ConnectivityStatus status_{};
  bool onlineEstablished_ = false;
};
