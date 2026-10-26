#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "ConfigValidator.h"
#include "ConnectivityManager.h"
#include "DeliveryManager.h"
#include "MessageFormatter.h"
#include "MonotonicClock.h"
#include "NtfyClient.h"
#include "NetworkDiagnostics.h"
#include "OfflineSummaryManager.h"
#include "RecoveryCompletionPolicy.h"
#include "PersistentStore.h"
#include "SessionManager.h"
#include "TimeKeeper.h"
#include "TlsManager.h"
#include "WifiManager.h"

class DoorbellController {
 public:
  using InputPump = void (*)();
  using InputActivityProbe = bool (*)();

  DoorbellController();

  void begin();
  void tick(InputPump inputPump, InputActivityProbe inputActivityProbe);
  void onValidBell();

  uint64_t nowMs();
  ConnectivityStatus connectivityStatus() const;
  SessionStatus sessionStatus() const;
  OfflineSummaryStatus offlineSummaryStatus() const;
  bool setDiagnosticPc(const String& ipv4Text, uint16_t port);
  void clearDiagnosticPc();
  void printDiagnosticConfiguration() const;
  void runNetworkDiagnostics(InputPump inputPump,
                             bool automatic = false,
                             const __FlashStringHelper* triggerName = nullptr);
  void printWifiSnapshot() const;
  void printWifiHistory() const;
  void printWifiConfiguration() const;
  bool forceWifiReconnect();

 private:
  void updateEnvironment(uint64_t nowMs);
  void handleWifiHealthFault(uint64_t nowMs, InputPump inputPump);
  void restorePersistedSessionIfPossible(uint64_t nowMs);
  void abandonUnrestorableSession(uint64_t nowMs);
  void processSessionEvents(uint64_t nowMs);
  void processDelivery(uint64_t nowMs, InputPump inputPump);
  void processOfflineRecovery(uint64_t nowMs,
                              InputPump inputPump,
                              InputActivityProbe inputActivityProbe);
  void enterOffline(uint64_t nowMs);
  void recordOfflineBell(uint64_t nowMs);
  void closeDeliveryForSession(const SessionStatus& session,
                               uint64_t nowMs);

  bool persistState();
  void generateSequenceId(NotificationStage stage,
                          char* output,
                          size_t outputSize);
  void printServiceStatus(uint64_t nowMs);
  const __FlashStringHelper* connectivityName(ConnectivityState state) const;
  const __FlashStringHelper* stageName(NotificationStage stage) const;
  const __FlashStringHelper* resultName(NtfyResultCode code) const;

  MonotonicClock clock_{};
  PersistentStore store_{};
  WifiManager wifi_{};
  TimeKeeper timeKeeper_{};
  TlsManager tls_{};
  NtfyClient ntfy_;
  NetworkDiagnostics networkDiagnostics_;
  MessageFormatter formatter_{};
  OfflineSummaryManager offlineSummary_{};
  SessionManager sessions_{};
  DeliveryManager deliveries_{};
  ConnectivityManager connectivity_{};

  PersistentStateRecord loadedRecord_{};
  PersistentStateRecord saveRecord_{};
  bool loadedRecordValid_ = false;
  bool sessionRestorePending_ = false;
  bool configurationValid_ = false;
  bool stateDirty_ = false;
  bool networkOperationActive_ = false;
  bool persistenceDeferredDuringNetwork_ = false;
  uint32_t stateSaveFailures_ = 0;
  uint32_t sequenceCounter_ = 0;
  uint32_t bootNonce_ = 0;
  uint64_t lastServiceStatusMs_ = 0;
  uint64_t nextOfflineRecoveryAttemptMs_ = 0;
  bool lastWifiConnected_ = false;
  bool recoveryPublishAwaitingInputSettle_ = false;
  uint32_t recoveryAcknowledgedPresses_ = 0;
  uint32_t recoveryAcknowledgedOccasions_ = 0;
};
