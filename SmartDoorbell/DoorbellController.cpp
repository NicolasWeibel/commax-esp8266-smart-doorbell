#include "DoorbellController.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include <user_interface.h>
}

#include <ESP8266WiFi.h>
#include <Esp.h>

#include "AppConfig.h"
#include "Secrets.h"

DoorbellController::DoorbellController()
    : ntfy_(tls_), networkDiagnostics_(tls_) {}

void DoorbellController::begin() {
  const uint64_t now = clock_.nowMs();
  bootNonce_ = os_random();
  configurationValid_ = ConfigValidator::valid();

  const bool filesystemReady = store_.begin();
  const bool tlsReady = tls_.begin(filesystemReady);
  const bool endpointReady = ntfy_.begin();

  timeKeeper_.begin(now);
  sessions_.begin();
  deliveries_.begin();
  offlineSummary_.begin(bootNonce_);

  if (filesystemReady && store_.load(loadedRecord_)) {
    loadedRecordValid_ = true;
    deliveries_.restore(loadedRecord_.jobs,
                        kPersistentDeliveryJobCapacity,
                        now);
    offlineSummary_.restore(loadedRecord_.offline, bootNonce_);
    const LastBellTimeQuality persistedQuality =
        static_cast<LastBellTimeQuality>(
            loadedRecord_.offline.lastBellTimeQuality);
    if (loadedRecord_.offline.active != 0U &&
        persistedQuality !=
            offlineSummary_.status().lastBellTimeQuality) {
      stateDirty_ = true;
    }
    sessionRestorePending_ =
        !offlineSummary_.active() && loadedRecord_.session.active != 0U;
  }

  connectivity_.begin(now, offlineSummary_.active());
  nextOfflineRecoveryAttemptMs_ = now;

  if (configurationValid_) {
    wifi_.begin(now);
  }

  Serial.print(F("[INFO] CONFIG | valid="));
  Serial.println(configurationValid_ ? F("yes") : F("NO"));
  Serial.print(F("[INFO] FILESYSTEM | ready="));
  Serial.println(filesystemReady ? F("yes") : F("no"));
  Serial.print(F("[INFO] TLS | ready="));
  Serial.print(tlsReady ? F("yes") : F("no"));
  Serial.print(F(" | certs="));
  Serial.print(tls_.certificateCount());
  Serial.print(F(" | endpoint="));
  Serial.println(endpointReady ? F("ready") : F("invalid"));
  Serial.print(F("[INFO] RESTORE | record="));
  Serial.print(loadedRecordValid_ ? F("valid") : F("none"));
  Serial.print(F(" | migrated_v3="));
  Serial.print(store_.migratedLegacy() ? F("yes") : F("no"));
  Serial.print(F(" | pending_jobs="));
  Serial.print(deliveries_.status(now).usedJobs);
  Serial.print(F(" | active_session_pending="));
  Serial.print(sessionRestorePending_ ? F("yes") : F("no"));
  Serial.print(F(" | offline_summary="));
  Serial.print(offlineSummary_.active() ? F("active") : F("none"));
  Serial.print(F(" | offline_presses="));
  Serial.println(offlineSummary_.status().totalPresses);

  if (store_.migratedLegacy()) {
    stateDirty_ = true;
  }
}

void DoorbellController::tick(InputPump inputPump,
                              InputActivityProbe inputActivityProbe) {
  const uint64_t now = clock_.nowMs();
  updateEnvironment(now);
  handleWifiHealthFault(now, inputPump);
  ntfy_.tick(now, wifi_.connected());
  restorePersistedSessionIfPossible(now);
  offlineSummary_.tick(now);

  if (offlineSummary_.active()) {
    processOfflineRecovery(clock_.nowMs(), inputPump, inputActivityProbe);
  } else {
    sessions_.tick(now);
    processSessionEvents(now);
    processDelivery(clock_.nowMs(), inputPump);
  }
  ntfy_.tick(clock_.nowMs(), wifi_.connected());

  if (stateDirty_) {
    persistState();
  }
  printServiceStatus(clock_.nowMs());
}

void DoorbellController::onValidBell() {
  uint64_t now = clock_.nowMs();

  const ConnectivityState connectionState = connectivity_.status().state;
  if (offlineSummary_.active() ||
      connectionState == ConnectivityState::Offline ||
      connectionState == ConnectivityState::Recovering) {
    if (!offlineSummary_.active()) {
      enterOffline(now);
    }
    recordOfflineBell(now);
    return;
  }

  if (sessionRestorePending_) {
    // A new physical event is more trustworthy than an old active session
    // whose timing cannot yet be reconstructed. Preserve its delivery job but
    // close the old session before starting the new one.
    abandonUnrestorableSession(now);
  }

  sessions_.tick(now);
  processSessionEvents(now);

  const SessionStatus before = sessions_.status();
  const bool startsNew = !before.active;
  const bool startsInsistence =
      before.active && before.phase != SessionPhase::Insistence &&
      now >= before.firstWindowEndsMs;

  char newSequenceId[64]{};
  if (startsNew) {
    generateSequenceId(NotificationStage::FirstMinute,
                       newSequenceId,
                       sizeof(newSequenceId));
  } else if (startsInsistence) {
    generateSequenceId(NotificationStage::Insistence,
                       newSequenceId,
                       sizeof(newSequenceId));
  }

  const bool epochValid = timeKeeper_.valid();
  const time_t epoch = timeKeeper_.nowEpoch(now);
  const BellSessionResult result = sessions_.onValidBell(
      now, epochValid, epoch, newSequenceId);
  const SessionStatus after = sessions_.status();

  bool queued = false;
  switch (result) {
    case BellSessionResult::StartedFirst:
      queued = deliveries_.ensureSequence(
          NotificationStage::FirstMinute,
          after.firstSequenceId,
          after.totalPresses,
          after.lastPressEpochValid,
          after.lastPressEpoch,
          true,
          now);
      break;

    case BellSessionResult::UpdatedFirst:
      queued = deliveries_.updateSequence(
          after.firstSequenceId,
          after.totalPresses,
          after.lastPressEpochValid,
          after.lastPressEpoch,
          now);
      break;

    case BellSessionResult::StartedInsistence: {
      uint32_t confirmedThrough = 0U;
      deliveries_.supersedeSequence(before.firstSequenceId,
                                    &confirmedThrough);
      queued = deliveries_.ensureSequence(
          NotificationStage::Insistence,
          after.insistenceSequenceId,
          after.totalPresses,
          after.lastPressEpochValid,
          after.lastPressEpoch,
          true,
          now,
          confirmedThrough);
      break;
    }

    case BellSessionResult::UpdatedInsistence:
      queued = deliveries_.updateSequence(
          after.insistenceSequenceId,
          after.totalPresses,
          after.lastPressEpochValid,
          after.lastPressEpoch,
          now);
      break;
  }

  Serial.print(F("[INFO] SESSION | bell="));
  switch (result) {
    case BellSessionResult::StartedFirst:
      Serial.print(F("started_first"));
      break;
    case BellSessionResult::UpdatedFirst:
      Serial.print(F("updated_first"));
      break;
    case BellSessionResult::StartedInsistence:
      Serial.print(F("started_insistence"));
      break;
    case BellSessionResult::UpdatedInsistence:
      Serial.print(F("updated_insistence"));
      break;
  }
  Serial.print(F(" | total="));
  Serial.print(after.totalPresses);
  Serial.print(F(" | queue="));
  Serial.println(queued ? F("ok") : F("FAILED"));

  stateDirty_ = true;
  if (networkOperationActive_) {
    if (!persistenceDeferredDuringNetwork_) {
      Serial.println(F("[INFO] PERSISTENCE | deferred_during_network"));
    }
    persistenceDeferredDuringNetwork_ = true;
  } else {
    persistState();
  }
}

uint64_t DoorbellController::nowMs() {
  return clock_.nowMs();
}

ConnectivityStatus DoorbellController::connectivityStatus() const {
  return connectivity_.status();
}

SessionStatus DoorbellController::sessionStatus() const {
  return sessions_.status();
}

OfflineSummaryStatus DoorbellController::offlineSummaryStatus() const {
  return offlineSummary_.status();
}

bool DoorbellController::setDiagnosticPc(const String& ipv4Text,
                                         uint16_t port) {
  return networkDiagnostics_.setLocalTarget(ipv4Text, port);
}

void DoorbellController::clearDiagnosticPc() {
  networkDiagnostics_.clearLocalTarget();
}

void DoorbellController::printDiagnosticConfiguration() const {
  networkDiagnostics_.printConfiguration();
}

void DoorbellController::runNetworkDiagnostics(
    InputPump inputPump,
    bool automatic,
    const __FlashStringHelper* triggerName) {
  ntfy_.close();
  networkOperationActive_ = true;
  networkDiagnostics_.run(clock_.nowMs(),
                          ntfy_.host(),
                          ntfy_.port(),
                          timeKeeper_.valid(),
                          timeKeeper_.nowEpoch(clock_.nowMs()),
                          inputPump,
                          automatic,
                          triggerName);
  networkOperationActive_ = false;
  if (persistenceDeferredDuringNetwork_) {
    Serial.println(F("[INFO] PERSISTENCE | deferred_state_ready_to_flush"));
    persistenceDeferredDuringNetwork_ = false;
  }
}


void DoorbellController::printWifiSnapshot() const {
  wifi_.printSnapshot(F("manual"));
}

void DoorbellController::printWifiHistory() const {
  wifi_.printHistory();
}

void DoorbellController::printWifiConfiguration() const {
  wifi_.printConfiguration();
}

bool DoorbellController::forceWifiReconnect() {
  ntfy_.close();
  return wifi_.forceReconnect(clock_.nowMs(), true);
}

void DoorbellController::handleWifiHealthFault(uint64_t nowMs,
                                               InputPump inputPump) {
  if (!wifi_.consumeInvalidIpv4Detected()) {
    return;
  }

  ntfy_.close();
  wifi_.printSnapshot(F("associated_without_valid_ipv4"));
  wifi_.printFaultAssessment();

  if (AppConfig::kEnableAutomaticLayeredDiagnostics &&
      networkDiagnostics_.automaticDue(clock_.nowMs())) {
    runNetworkDiagnostics(inputPump,
                          true,
                          F("automatic_invalid_ipv4"));
  }

  const bool recycled = wifi_.recoverInvalidIpv4(nowMs);
  Serial.print(F("[WARN] WIFI | invalid_ipv4_recovery | executed="));
  Serial.println(recycled ? F("yes") : F("no"));
}

void DoorbellController::updateEnvironment(uint64_t nowMs) {
  if (configurationValid_) {
    wifi_.tick(nowMs);
  }

  const bool wifiConnected = configurationValid_ && wifi_.connected();
  const bool wifiReturned = !lastWifiConnected_ && wifiConnected;

  timeKeeper_.tick(nowMs, wifiConnected);
  const bool newlySynchronized = timeKeeper_.consumeNewSynchronization();
  if (newlySynchronized) {
    const time_t epoch = timeKeeper_.nowEpoch(nowMs);
    char localText[24]{};
    char utcText[24]{};
    timeKeeper_.formatLocal(epoch, localText, sizeof(localText));
    struct tm utcTime{};
    if (gmtime_r(&epoch, &utcTime) != nullptr) {
      strftime(utcText, sizeof(utcText), "%Y-%m-%d %H:%M", &utcTime);
    }
    Serial.print(F("[INFO] TIME | synchronized | utc="));
    Serial.print(utcText[0] != '\0' ? utcText : "unknown");
    Serial.print(F(" | local="));
    Serial.print(localText[0] != '\0' ? localText : "unknown");
    Serial.print(F(" | timezone="));
    Serial.println(Secrets::kTimeZone);
  }

  ntfy_.setEnvironment(wifiConnected,
                       timeKeeper_.valid(),
                       timeKeeper_.nowEpoch(nowMs));
  connectivity_.updateEnvironment(wifiConnected,
                                  timeKeeper_.valid(),
                                  nowMs);

  if (connectivity_.consumeBecameUnavailable()) {
    const ConnectivityStatus status = connectivity_.status();
    enterOffline(nowMs);
    wifi_.printSnapshot(F("connectivity_entered_offline"));
    wifi_.printFaultAssessment();
    Serial.print(F("[WARN] CONNECTIVITY | OFFLINE | reason="));
    switch (status.offlineReason) {
      case OfflineReason::WifiLost:
        Serial.print(F("wifi_lost"));
        break;
      case OfflineReason::PublishFailures:
        Serial.print(F("four_publish_failures"));
        break;
      case OfflineReason::FailureTimeout:
        Serial.print(F("failure_series_60s"));
        break;
      case OfflineReason::TimeUnavailable:
        Serial.print(F("time_unavailable_60s"));
        break;
      case OfflineReason::RestoredState:
        Serial.print(F("restored_state"));
        break;
      case OfflineReason::None:
        Serial.print(F("unknown"));
        break;
    }
    Serial.print(F(" | failures="));
    Serial.print(status.consecutiveFailures);
    Serial.print(F(" | wifi="));
    Serial.println(wifiConnected ? F("connected") : F("disconnected"));
  }

  if (offlineSummary_.active() && timeKeeper_.valid() &&
      (wifiReturned || newlySynchronized)) {
    nextOfflineRecoveryAttemptMs_ = nowMs;
    Serial.println(F("[INFO] CONNECTIVITY | recovery_probe_armed"));
  }

  if (connectivity_.consumeRecovered()) {
    Serial.println(F("[INFO] CONNECTIVITY | ONLINE_RECOVERED"));
  }
  lastWifiConnected_ = wifiConnected;
}

void DoorbellController::restorePersistedSessionIfPossible(uint64_t nowMs) {
  if (!sessionRestorePending_ || !loadedRecordValid_ ||
      !timeKeeper_.valid()) {
    return;
  }

  const bool restored = sessions_.restore(
      loadedRecord_.session,
      nowMs,
      true,
      timeKeeper_.nowEpoch(nowMs));
  sessionRestorePending_ = false;
  if (restored) {
    Serial.print(F("[INFO] RESTORE | active_session=yes | total="));
    Serial.println(sessions_.status().totalPresses);
  } else {
    Serial.println(F("[INFO] RESTORE | active_session=expired_or_unknown"));
    const PersistentSessionState& old = loadedRecord_.session;
    const NotificationStage stage =
        static_cast<SessionPhase>(old.phase) == SessionPhase::Insistence
            ? NotificationStage::Insistence
            : NotificationStage::FirstMinute;
    const char* sequenceId =
        stage == NotificationStage::Insistence
            ? old.insistenceSequenceId
            : old.firstSequenceId;
    deliveries_.closeSequence(
        sequenceId,
        old.totalPresses,
        old.lastPressEpochValid != 0U,
        static_cast<time_t>(old.lastPressEpoch),
        nowMs);
  }
  stateDirty_ = true;
}

void DoorbellController::abandonUnrestorableSession(uint64_t nowMs) {
  if (!sessionRestorePending_ || !loadedRecordValid_) {
    return;
  }

  const PersistentSessionState& old = loadedRecord_.session;
  const NotificationStage stage =
      static_cast<SessionPhase>(old.phase) == SessionPhase::Insistence
          ? NotificationStage::Insistence
          : NotificationStage::FirstMinute;
  const char* sequenceId =
      stage == NotificationStage::Insistence
          ? old.insistenceSequenceId
          : old.firstSequenceId;
  deliveries_.closeSequence(
      sequenceId,
      old.totalPresses,
      old.lastPressEpochValid != 0U,
      static_cast<time_t>(old.lastPressEpoch),
      nowMs);
  sessions_.reset();
  sessionRestorePending_ = false;
  stateDirty_ = true;
  Serial.println(F("[WARN] RESTORE | old_session_closed_before_new_bell"));
}

void DoorbellController::processSessionEvents(uint64_t nowMs) {
  SessionEvent event{};
  while (sessions_.popEvent(event)) {
    if (event.type == SessionEventType::FirstWindowClosed) {
      deliveries_.forceSequence(event.sequenceId,
                                event.totalPresses,
                                event.lastPressEpochValid,
                                event.lastPressEpoch,
                                nowMs);
      Serial.print(F("[INFO] SESSION | first_minute_closed | total="));
      Serial.println(event.totalPresses);
    } else {
      deliveries_.closeSequence(event.sequenceId,
                                event.totalPresses,
                                event.lastPressEpochValid,
                                event.lastPressEpoch,
                                nowMs);
      Serial.print(F("[INFO] SESSION | expired_after_silence | total="));
      Serial.println(event.totalPresses);
    }
    stateDirty_ = true;
  }
}

void DoorbellController::processDelivery(uint64_t nowMs,
                                         InputPump inputPump) {
  const ConnectivityState connectionState = connectivity_.status().state;
  if (offlineSummary_.active() ||
      connectionState == ConnectivityState::Offline ||
      connectionState == ConnectivityState::Recovering ||
      !configurationValid_ || !wifi_.connected() ||
      !timeKeeper_.valid() || !tls_.ready()) {
    return;
  }

  DeliveryAttempt attempt{};
  if (!deliveries_.nextDue(nowMs, attempt)) {
    return;
  }

  const String title = formatter_.title(attempt.stage);
  const String message = formatter_.message(
      attempt.stage,
      attempt.requestCount,
      attempt.lastPressEpochValid,
      attempt.lastPressEpoch,
      timeKeeper_.nowEpoch(nowMs),
      timeKeeper_);

  Serial.print(F("[INFO] NTFY | attempt_begin | stage="));
  Serial.print(stageName(attempt.stage));
  Serial.print(F(" | count="));
  Serial.print(attempt.requestCount);
  Serial.print(F(" | sequence="));
  Serial.print(attempt.sequenceId);
  Serial.println(F(" | transport=ESP8266HTTPClient | connection=keep-alive"));

  networkOperationActive_ = true;
  const NtfyPublishResult result =
      ntfy_.publish(attempt, title, message, inputPump);

  if (inputPump != nullptr) {
    inputPump();
  }

  uint8_t publishFailureNumber = 0;
  if (result.success) {
    deliveries_.recordSuccess(attempt, clock_.nowMs());
    connectivity_.recordPublishSuccess(clock_.nowMs());
  } else {
    deliveries_.recordFailure(attempt, clock_.nowMs());
    connectivity_.recordPublishFailure(clock_.nowMs());
    publishFailureNumber =
        connectivity_.status().consecutiveFailures;
  }
  networkOperationActive_ = false;
  if (persistenceDeferredDuringNetwork_) {
    Serial.println(F("[INFO] PERSISTENCE | deferred_state_ready_to_flush"));
    persistenceDeferredDuringNetwork_ = false;
  }

  if (!result.success && publishFailureNumber == 1U &&
      (result.code == NtfyResultCode::ConnectFailed ||
       result.code == NtfyResultCode::TlsHandshakeFailed)) {
    wifi_.printSnapshot(F("first_ntfy_connect_failure"));
    wifi_.printFaultAssessment();
    if (AppConfig::kEnableAutomaticLayeredDiagnostics &&
        networkDiagnostics_.automaticDue(clock_.nowMs())) {
      runNetworkDiagnostics(inputPump,
                            true,
                            F("automatic_ntfy_failure"));
    }
  }

  if (result.wifiRecycleRequested &&
      connectivity_.status().state != ConnectivityState::Offline) {
    ntfy_.close();
    const bool recycled = wifi_.forceReconnect(clock_.nowMs());
    Serial.print(F("[WARN] WIFI | forced_recycle | executed="));
    Serial.println(recycled ? F("yes") : F("cooldown"));
  }

  const bool detailedResult =
      result.success || publishFailureNumber <= 1U ||
      publishFailureNumber >= AppConfig::kOfflineAfterFailures ||
      result.httpStatus > 0;

  if (!detailedResult) {
    Serial.print(F("[WARN] NTFY | success=no | failure="));
    Serial.print(publishFailureNumber);
    Serial.print('/');
    Serial.print(AppConfig::kOfflineAfterFailures);
    Serial.print(F(" | result="));
    Serial.print(resultName(result.code));
    Serial.print(F(" | total_ms="));
    Serial.println(result.totalMs);
  } else {
    Serial.print(result.success ? F("[INFO] NTFY | success=yes")
                                : F("[WARN] NTFY | success=no"));
    if (!result.success) {
      Serial.print(F(" | failure="));
      Serial.print(publishFailureNumber);
      Serial.print('/');
      Serial.print(AppConfig::kOfflineAfterFailures);
    }
    Serial.print(F(" | result="));
    Serial.print(resultName(result.code));
    Serial.print(F(" | transport=ESP8266HTTPClient"));
    Serial.print(F(" | http="));
    Serial.print(result.httpStatus);
    Serial.print(F(" | client_error="));
    Serial.print(result.httpClientError);
    Serial.print(F(" | ssl="));
    Serial.print(result.sslError);
    Serial.print(F(" | response_bytes="));
    Serial.print(result.responseBytes);
    Serial.print(F(" | connection="));
    Serial.print(result.connectionReused ? F("reused") : F("new"));
    Serial.print(F(" | reconnect="));
    if (!result.reconnectAttempted) {
      Serial.print(F("no"));
    } else {
      Serial.print(result.reconnectSucceeded ? F("success") : F("failed"));
    }
    Serial.print(F(" | keepalive="));
    Serial.print(result.keepAliveRetained ? F("retained") : F("closed"));
    Serial.print(F(" | generation="));
    Serial.print(result.transportGeneration);
    Serial.print(F(" | local_port="));
    Serial.print(result.localPort);
    Serial.print(F(" | tcp_time_wait="));
    Serial.print(result.tcpTimeWaitBefore);
    Serial.print(F("->"));
    Serial.print(result.tcpTimeWaitAfter);
    Serial.print(F(" | wifi_recycle="));
    Serial.print(result.wifiRecycleRequested ? F("requested") : F("no"));
    Serial.print(F(" | total_ms="));
    Serial.print(result.totalMs);
    if (result.error.length() > 0U) {
      Serial.print(F(" | error="));
      Serial.print(result.error);
    }
    Serial.println();
  }

  if (!result.success &&
      connectivity_.status().state == ConnectivityState::Offline) {
    enterOffline(clock_.nowMs());
  }

  stateDirty_ = true;
}

void DoorbellController::processOfflineRecovery(
    uint64_t nowMs,
    InputPump inputPump,
    InputActivityProbe inputActivityProbe) {
  if (!offlineSummary_.active()) {
    recoveryPublishAwaitingInputSettle_ = false;
    recoveryAcknowledgedPresses_ = 0;
    recoveryAcknowledgedOccasions_ = 0;
    return;
  }

  if (nowMs < nextOfflineRecoveryAttemptMs_) {
    return;
  }

  if (recoveryPublishAwaitingInputSettle_) {
    if (inputPump != nullptr) {
      inputPump();
    }
    const bool inputBusy =
        inputActivityProbe != nullptr && inputActivityProbe();
    if (inputBusy) {
      nextOfflineRecoveryAttemptMs_ =
          clock_.nowMs() + AppConfig::kRecoveryInputSettlePollMs;
      return;
    }

    const OfflineSummaryStatus settled = offlineSummary_.status();
    const bool changedAfterAcknowledgement =
        settled.totalPresses != recoveryAcknowledgedPresses_ ||
        settled.occasionCount != recoveryAcknowledgedOccasions_;
    recoveryPublishAwaitingInputSettle_ = false;

    const RecoveryCompletionAction action =
        RecoveryCompletionPolicy::afterInputSettles(
            changedAfterAcknowledgement);
    if (action == RecoveryCompletionAction::ScheduleUpdate) {
      nextOfflineRecoveryAttemptMs_ =
          clock_.nowMs() + AppConfig::kNotificationUpdateIntervalMs;
      Serial.print(F("[INFO] OFFLINE_SUMMARY | input_settled=yes | update_pending=yes | presses="));
      Serial.print(settled.totalPresses);
      Serial.print(F(" | retry_in_ms="));
      Serial.println(AppConfig::kNotificationUpdateIntervalMs);
    } else {
      offlineSummary_.clear();
      connectivity_.recordPublishSuccess(clock_.nowMs());
      nextOfflineRecoveryAttemptMs_ = 0;
      Serial.println(F("[INFO] OFFLINE_SUMMARY | sent_finalized_after_input_settled=yes"));
    }
    stateDirty_ = true;
    return;
  }

  if (!configurationValid_ || !wifi_.connected() ||
      !timeKeeper_.valid() || !tls_.ready()) {
    return;
  }

  if (inputActivityProbe != nullptr && inputActivityProbe()) {
    // Do not begin or repeat a recovery publication while a physical pulse is
    // still open. The falling edge is what validates the bell and updates the
    // persistent summary.
    nextOfflineRecoveryAttemptMs_ =
        nowMs + AppConfig::kRecoveryInputSettlePollMs;
    return;
  }

  const OfflineSummaryStatus summary = offlineSummary_.status();
  if (!offlineSummary_.hasBells()) {
    IPAddress resolved;
    const bool dnsReady = WiFi.hostByName(ntfy_.host().c_str(), resolved) == 1;
    if (!dnsReady) {
      connectivity_.recordRecoveryFailure(nowMs);
      nextOfflineRecoveryAttemptMs_ =
          nowMs + AppConfig::kOfflineRecoveryProbeIntervalMs;
      Serial.println(F("[WARN] OFFLINE_SUMMARY | no_bells_but_dns_unavailable"));
      return;
    }
    offlineSummary_.clear();
    connectivity_.recordPublishSuccess(nowMs);
    stateDirty_ = true;
    Serial.print(F("[INFO] OFFLINE_SUMMARY | recovered_without_bells | ntfy_ip="));
    Serial.println(resolved);
    return;
  }

  uint64_t ageSeconds = 0;
  const bool ageAvailable = offlineSummary_.lastBellAgeSeconds(
      nowMs, timeKeeper_.nowEpoch(nowMs), ageSeconds);
  const String title = formatter_.title(NotificationStage::RecoverySummary);
  const String message = formatter_.recoveryMessage(
      summary, ageAvailable, ageSeconds, timeKeeper_.nowEpoch(nowMs), timeKeeper_);

  DeliveryAttempt attempt{};
  attempt.valid = true;
  attempt.stage = NotificationStage::RecoverySummary;
  attempt.requestCount = summary.totalPresses;
  attempt.firstSuccessfulPublish = !summary.initialConfirmed;
  attempt.urgent = true;
  attempt.startedMs = nowMs;
  std::strncpy(attempt.sequenceId,
               summary.sequenceId,
               sizeof(attempt.sequenceId) - 1U);

  connectivity_.beginRecovery(nowMs);
  Serial.print(F("[INFO] OFFLINE_SUMMARY | attempt_begin | presses="));
  Serial.print(summary.totalPresses);
  Serial.print(F(" | occasions="));
  Serial.print(summary.occasionCount);
  Serial.print(F(" | sequence="));
  Serial.println(summary.sequenceId);

  networkOperationActive_ = true;
  const NtfyPublishResult result =
      ntfy_.publish(attempt, title, message, inputPump);
  networkOperationActive_ = false;
  if (persistenceDeferredDuringNetwork_) {
    persistenceDeferredDuringNetwork_ = false;
  }

  // Drain any edge captured between HTTPClient's last cooperative pump and the
  // completed HTTP response before deciding whether the summary is final.
  if (inputPump != nullptr) {
    inputPump();
  }

  if (result.success) {
    const OfflineSummaryStatus afterPublish = offlineSummary_.status();
    const bool changedDuringPublish =
        afterPublish.totalPresses != summary.totalPresses ||
        afterPublish.occasionCount != summary.occasionCount;
    const bool inputBusy =
        inputActivityProbe != nullptr && inputActivityProbe();
    const RecoveryCompletionAction action =
        RecoveryCompletionPolicy::afterSuccessfulPublish(
            inputBusy, changedDuringPublish);

    if (action == RecoveryCompletionAction::WaitForInput) {
      offlineSummary_.markInitialConfirmed();
      recoveryPublishAwaitingInputSettle_ = true;
      recoveryAcknowledgedPresses_ = summary.totalPresses;
      recoveryAcknowledgedOccasions_ = summary.occasionCount;
      nextOfflineRecoveryAttemptMs_ =
          clock_.nowMs() + AppConfig::kRecoveryInputSettlePollMs;
      Serial.print(F("[INFO] OFFLINE_SUMMARY | sent_deferred=yes | http="));
      Serial.print(result.httpStatus);
      Serial.print(F(" | input_in_flight=yes | updated_presses="));
      Serial.print(afterPublish.totalPresses);
      Serial.print(F(" | settle_poll_ms="));
      Serial.println(AppConfig::kRecoveryInputSettlePollMs);
    } else if (action == RecoveryCompletionAction::ScheduleUpdate) {
      offlineSummary_.markInitialConfirmed();
      nextOfflineRecoveryAttemptMs_ =
          clock_.nowMs() + AppConfig::kNotificationUpdateIntervalMs;
      Serial.print(F("[INFO] OFFLINE_SUMMARY | sent_partial=yes | http="));
      Serial.print(result.httpStatus);
      Serial.print(F(" | updated_presses="));
      Serial.print(afterPublish.totalPresses);
      Serial.print(F(" | retry_in_ms="));
      Serial.println(AppConfig::kNotificationUpdateIntervalMs);
    } else {
      offlineSummary_.clear();
      connectivity_.recordPublishSuccess(clock_.nowMs());
      nextOfflineRecoveryAttemptMs_ = 0;
      Serial.print(F("[INFO] OFFLINE_SUMMARY | sent=yes | http="));
      Serial.print(result.httpStatus);
      Serial.print(F(" | total_ms="));
      Serial.println(result.totalMs);
    }
  } else {
    connectivity_.recordRecoveryFailure(clock_.nowMs());
    nextOfflineRecoveryAttemptMs_ =
        clock_.nowMs() + AppConfig::kOfflineRecoveryProbeIntervalMs;
    Serial.print(F("[WARN] OFFLINE_SUMMARY | sent=no | result="));
    Serial.print(resultName(result.code));
    Serial.print(F(" | next_retry_ms="));
    Serial.println(AppConfig::kOfflineRecoveryProbeIntervalMs);
  }
  stateDirty_ = true;
}

void DoorbellController::enterOffline(uint64_t nowMs) {
  if (offlineSummary_.active()) {
    return;
  }

  char summarySequence[64]{};
  generateSequenceId(NotificationStage::RecoverySummary,
                     summarySequence,
                     sizeof(summarySequence));
  offlineSummary_.activate(summarySequence);
  recoveryPublishAwaitingInputSettle_ = false;
  recoveryAcknowledgedPresses_ = 0;
  recoveryAcknowledgedOccasions_ = 0;

  const SessionStatus activeSession = sessions_.status();
  const bool pendingRestoredSession =
      sessionRestorePending_ && loadedRecordValid_;
  const char* activeSequence = nullptr;
  if (activeSession.active) {
    activeSequence = activeSession.phase == SessionPhase::Insistence
                         ? activeSession.insistenceSequenceId
                         : activeSession.firstSequenceId;
  }
  const char* restoredSequence = nullptr;
  if (pendingRestoredSession) {
    const PersistentSessionState& old = loadedRecord_.session;
    restoredSequence =
        static_cast<SessionPhase>(old.phase) == SessionPhase::Insistence
            ? old.insistenceSequenceId
            : old.firstSequenceId;
  }
  const bool activeRepresented =
      activeSequence != nullptr && deliveries_.containsSequence(activeSequence);
  const bool restoredRepresented =
      restoredSequence != nullptr &&
      deliveries_.containsSequence(restoredSequence);

  const DeliveryAggregate aggregate = deliveries_.collapseUnreported();
  offlineSummary_.addHistorical(aggregate.totalPresses,
                                aggregate.occasionCount,
                                aggregate.latestEpochValid,
                                aggregate.latestEpoch);

  if (activeSession.active && !activeRepresented) {
    offlineSummary_.addHistorical(activeSession.totalPresses,
                                  1U,
                                  activeSession.lastPressEpochValid,
                                  activeSession.lastPressEpoch);
  }
  if (pendingRestoredSession && !restoredRepresented) {
    const PersistentSessionState& old = loadedRecord_.session;
    offlineSummary_.addHistorical(
        old.totalPresses,
        1U,
        old.lastPressEpochValid != 0U,
        static_cast<time_t>(old.lastPressEpoch));
  }

  if (activeSession.active) {
    if (activeSession.lastPressEpochValid &&
        activeSession.lastPressEpoch > 0) {
      offlineSummary_.setLatestExact(activeSession.lastPressEpoch);
    } else {
      offlineSummary_.setLatestRelative(activeSession.lastPressMs);
    }
  } else if (pendingRestoredSession) {
    const PersistentSessionState& old = loadedRecord_.session;
    if (old.lastPressEpochValid != 0U && old.lastPressEpoch > 0) {
      offlineSummary_.setLatestExact(
          static_cast<time_t>(old.lastPressEpoch));
    } else {
      offlineSummary_.setLatestUnknown();
    }
  } else if (aggregate.totalPresses > 0U &&
             !aggregate.latestEpochValid) {
    offlineSummary_.setLatestUnknown();
  }

  sessions_.reset();
  sessionRestorePending_ = false;
  ntfy_.close();
  nextOfflineRecoveryAttemptMs_ =
      nowMs + AppConfig::kOfflineRecoveryProbeIntervalMs;
  stateDirty_ = true;

  const OfflineSummaryStatus summary = offlineSummary_.status();
  Serial.print(F("[WARN] OFFLINE_SUMMARY | created | presses="));
  Serial.print(summary.totalPresses);
  Serial.print(F(" | occasions="));
  Serial.print(summary.occasionCount);
  Serial.print(F(" | recovery_in_ms="));
  Serial.println(AppConfig::kOfflineRecoveryProbeIntervalMs);
}

void DoorbellController::recordOfflineBell(uint64_t nowMs) {
  const bool epochValid = timeKeeper_.valid();
  offlineSummary_.recordBell(nowMs,
                             epochValid,
                             timeKeeper_.nowEpoch(nowMs));
  const OfflineSummaryStatus summary = offlineSummary_.status();
  Serial.print(F("[INFO] OFFLINE_BELL | total="));
  Serial.print(summary.totalPresses);
  Serial.print(F(" | occasions="));
  Serial.print(summary.occasionCount);
  Serial.print(F(" | time_quality="));
  switch (summary.lastBellTimeQuality) {
    case LastBellTimeQuality::ExactEpoch:
      Serial.println(F("exact"));
      break;
    case LastBellTimeQuality::RelativeUptime:
      Serial.println(F("relative_uptime"));
      break;
    case LastBellTimeQuality::UnknownAfterRestart:
      Serial.println(F("unknown_after_restart"));
      break;
    case LastBellTimeQuality::None:
      Serial.println(F("none"));
      break;
  }
  stateDirty_ = true;
  if (networkOperationActive_) {
    persistenceDeferredDuringNetwork_ = true;
  } else {
    persistState();
  }
}

void DoorbellController::closeDeliveryForSession(
    const SessionStatus& session,
    uint64_t nowMs) {
  if (!session.active) {
    return;
  }
  const bool insistence = session.phase == SessionPhase::Insistence;
  deliveries_.closeSequence(
      insistence ? session.insistenceSequenceId : session.firstSequenceId,
      session.totalPresses,
      session.lastPressEpochValid,
      session.lastPressEpoch,
      nowMs);
}

bool DoorbellController::persistState() {
  if (!store_.ready()) {
    return false;
  }

  saveRecord_ = PersistentStateRecord{};
  saveRecord_.session = sessions_.persistentState();
  deliveries_.exportPersistent(saveRecord_.jobs,
                               kPersistentDeliveryJobCapacity);
  saveRecord_.offline = offlineSummary_.persistentState();
  if (AppConfig::kVerboseOperationalLogs) {
    Serial.print(F("[INFO] PERSISTENCE | save_begin | bytes="));
    Serial.println(sizeof(PersistentStateRecord));
  }
  yield();
  const bool saved = store_.save(saveRecord_);
  yield();
  if (saved) {
    stateDirty_ = false;
    if (AppConfig::kVerboseOperationalLogs) {
      Serial.println(F("[INFO] PERSISTENCE | save_ok"));
    }
  } else {
    ++stateSaveFailures_;
    Serial.print(F("[WARN] PERSISTENCE | save_failed | total="));
    Serial.println(stateSaveFailures_);
  }
  return saved;
}

void DoorbellController::generateSequenceId(NotificationStage stage,
                                            char* output,
                                            size_t outputSize) {
  if (output == nullptr || outputSize == 0U) {
    return;
  }

  ++sequenceCounter_;
  const time_t epoch = timeKeeper_.valid()
                           ? timeKeeper_.nowEpoch(clock_.nowMs())
                           : 0;
  std::snprintf(output,
                outputSize,
                "db-%06lx-%08lx-%08lx-%lu-%c",
                static_cast<unsigned long>(ESP.getChipId()),
                static_cast<unsigned long>(bootNonce_),
                static_cast<unsigned long>(epoch > 0
                                               ? static_cast<uint32_t>(epoch)
                                               : millis()),
                static_cast<unsigned long>(sequenceCounter_),
                stage == NotificationStage::Insistence
                    ? 'i'
                    : (stage == NotificationStage::RecoverySummary ? 'r' : 'f'));
}

void DoorbellController::printServiceStatus(uint64_t nowMs) {
  if ((nowMs - lastServiceStatusMs_) <
      AppConfig::kServiceStatusIntervalMs) {
    return;
  }
  lastServiceStatusMs_ = nowMs;

  const SessionStatus session = sessions_.status();
  const DeliveryStatus delivery = deliveries_.status(nowMs);
  const ConnectivityStatus connection = connectivity_.status();
  Serial.print(F("[INFO] SERVICE_STATUS | connectivity="));
  Serial.print(connectivityName(connection.state));
  Serial.print(F(" | wifi="));
  Serial.print(wifi_.connected() ? F("ready") : F("not_ready"));
  Serial.print(F(" | associated="));
  Serial.print(wifi_.associated() ? F("yes") : F("no"));
  Serial.print(F(" | ip="));
  Serial.print(wifi_.localIp());
  Serial.print(F(" | rssi="));
  Serial.print(wifi_.associated() ? WiFi.RSSI() : 0);
  Serial.print(F(" | session="));
  Serial.print(session.active ? F("active") : F("idle"));
  Serial.print(F(" | presses="));
  Serial.print(session.totalPresses);
  Serial.print(F(" | jobs="));
  Serial.print(delivery.usedJobs);
  Serial.print(F(" | due="));
  Serial.print(delivery.dueJobs);
  Serial.print(F(" | sent="));
  Serial.print(delivery.successfulPublishes);
  Serial.print(F(" | failed="));
  Serial.print(delivery.failedPublishes);
  const OfflineSummaryStatus offline = offlineSummary_.status();
  Serial.print(F(" | superseded="));
  Serial.print(delivery.supersededJobs);
  Serial.print(F(" | offline_presses="));
  Serial.print(offline.totalPresses);
  Serial.print(F(" | offline_occasions="));
  Serial.print(offline.occasionCount);
  Serial.print(F(" | heap="));
  Serial.println(ESP.getFreeHeap());
}

const __FlashStringHelper* DoorbellController::connectivityName(
    ConnectivityState state) const {
  switch (state) {
    case ConnectivityState::WaitingForTime:
      return F("WAITING_FOR_TIME");
    case ConnectivityState::Online:
      return F("ONLINE");
    case ConnectivityState::Degraded:
      return F("DEGRADED");
    case ConnectivityState::Offline:
      return F("OFFLINE");
    case ConnectivityState::Recovering:
      return F("RECOVERING");
  }
  return F("UNKNOWN");
}

const __FlashStringHelper* DoorbellController::stageName(
    NotificationStage stage) const {
  if (stage == NotificationStage::RecoverySummary) {
    return F("RECOVERY_SUMMARY");
  }
  return stage == NotificationStage::Insistence
             ? F("INSISTENCE")
             : F("FIRST_MINUTE");
}

const __FlashStringHelper* DoorbellController::resultName(
    NtfyResultCode code) const {
  switch (code) {
    case NtfyResultCode::Success:
      return F("Success");
    case NtfyResultCode::WifiUnavailable:
      return F("WifiUnavailable");
    case NtfyResultCode::TimeUnavailable:
      return F("TimeUnavailable");
    case NtfyResultCode::TlsUnavailable:
      return F("TlsUnavailable");
    case NtfyResultCode::InvalidEndpoint:
      return F("InvalidEndpoint");
    case NtfyResultCode::DnsFailed:
      return F("DnsFailed");
    case NtfyResultCode::ConnectFailed:
      return F("ConnectFailed");
    case NtfyResultCode::TlsHandshakeFailed:
      return F("TlsHandshakeFailed");
    case NtfyResultCode::WriteFailed:
      return F("WriteFailed");
    case NtfyResultCode::ResponseTimeout:
      return F("ResponseTimeout");
    case NtfyResultCode::InvalidResponse:
      return F("InvalidResponse");
    case NtfyResultCode::HttpError:
      return F("HttpError");
  }
  return F("Unknown");
}
