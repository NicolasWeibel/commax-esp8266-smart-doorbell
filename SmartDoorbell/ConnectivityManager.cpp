#include "ConnectivityManager.h"

#include "AppConfig.h"

void ConnectivityManager::begin(uint64_t nowMs, bool restoredOffline) {
  status_ = ConnectivityStatus{};
  onlineEstablished_ = false;
  status_.state = restoredOffline ? ConnectivityState::Offline
                                  : ConnectivityState::WaitingForTime;
  if (restoredOffline) {
    status_.offlineReason = OfflineReason::RestoredState;
    status_.offlineSinceMs = nowMs;
  }
}

void ConnectivityManager::updateEnvironment(bool wifiConnected,
                                            bool timeValid,
                                            uint64_t nowMs) {
  status_.wifiConnected = wifiConnected;

  if (!wifiConnected) {
    // During a normal boot the station may need more than five seconds to
    // associate and obtain DHCP. Do not create an empty offline summary before
    // the device has ever reached a verified ONLINE state.
    if (!onlineEstablished_ &&
        status_.state == ConnectivityState::WaitingForTime) {
      status_.wifiDisconnectedSinceMs = 0U;
      return;
    }
    if (status_.wifiDisconnectedSinceMs == 0U) {
      status_.wifiDisconnectedSinceMs = nowMs;
    } else if ((nowMs - status_.wifiDisconnectedSinceMs) >=
               AppConfig::kWifiOfflineConfirmationMs) {
      setOffline(OfflineReason::WifiLost, nowMs);
    }
    return;
  }

  status_.wifiDisconnectedSinceMs = 0;

  if (status_.state == ConnectivityState::WaitingForTime) {
    if (!timeValid) {
      if (status_.firstFailureMs == 0U) {
        status_.firstFailureMs = nowMs;
      } else if ((nowMs - status_.firstFailureMs) >= AppConfig::kOfflineFailureWindowMs) {
        setOffline(OfflineReason::TimeUnavailable, nowMs);
      }
      return;
    }
    status_.state = ConnectivityState::Online;
    onlineEstablished_ = true;
    status_.offlineReason = OfflineReason::None;
    resetFailureSeries();
  }

  if (status_.state == ConnectivityState::Degraded &&
      status_.firstFailureMs != 0U &&
      (nowMs - status_.firstFailureMs) >= AppConfig::kOfflineFailureWindowMs) {
    setOffline(OfflineReason::FailureTimeout, nowMs);
  }
}

void ConnectivityManager::recordPublishFailure(uint64_t nowMs) {
  if (status_.state == ConnectivityState::Offline ||
      status_.state == ConnectivityState::Recovering) {
    recordRecoveryFailure(nowMs);
    return;
  }

  if (!status_.wifiConnected) {
    setOffline(OfflineReason::WifiLost, nowMs);
    return;
  }

  if (status_.firstFailureMs == 0U) {
    status_.firstFailureMs = nowMs;
  }
  if (status_.consecutiveFailures < UINT8_MAX) {
    ++status_.consecutiveFailures;
  }

  if (status_.consecutiveFailures >= AppConfig::kOfflineAfterFailures) {
    setOffline(OfflineReason::PublishFailures, nowMs);
  } else {
    status_.state = ConnectivityState::Degraded;
  }
}

void ConnectivityManager::recordPublishSuccess(uint64_t nowMs) {
  (void)nowMs;
  const bool wasNotOnline = status_.state != ConnectivityState::Online;
  status_.state = ConnectivityState::Online;
  onlineEstablished_ = true;
  status_.offlineReason = OfflineReason::None;
  status_.offlineSinceMs = 0;
  resetFailureSeries();
  status_.recovered = wasNotOnline;
}

void ConnectivityManager::beginRecovery(uint64_t nowMs) {
  (void)nowMs;
  if (status_.state == ConnectivityState::Offline) {
    status_.state = ConnectivityState::Recovering;
  }
}

void ConnectivityManager::recordRecoveryFailure(uint64_t nowMs) {
  status_.state = ConnectivityState::Offline;
  if (status_.offlineSinceMs == 0U) {
    status_.offlineSinceMs = nowMs;
  }
}

ConnectivityStatus ConnectivityManager::status() const {
  return status_;
}

bool ConnectivityManager::consumeBecameUnavailable() {
  const bool value = status_.becameUnavailable;
  status_.becameUnavailable = false;
  return value;
}

bool ConnectivityManager::consumeRecovered() {
  const bool value = status_.recovered;
  status_.recovered = false;
  return value;
}

void ConnectivityManager::setOffline(OfflineReason reason, uint64_t nowMs) {
  const bool alreadyOffline =
      status_.state == ConnectivityState::Offline ||
      status_.state == ConnectivityState::Recovering;
  status_.state = ConnectivityState::Offline;
  status_.offlineReason = reason;
  if (status_.offlineSinceMs == 0U) {
    status_.offlineSinceMs = nowMs;
  }
  if (!alreadyOffline) {
    status_.becameUnavailable = true;
  }
}

void ConnectivityManager::resetFailureSeries() {
  status_.consecutiveFailures = 0;
  status_.firstFailureMs = 0;
}
