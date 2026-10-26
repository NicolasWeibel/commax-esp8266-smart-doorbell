#include <cassert>
#include <cstring>
#include <iostream>

#include "BellDetector.h"
#include "ConnectivityManager.h"
#include "DeliveryManager.h"
#include "OfflineSummaryManager.h"
#include "RecoveryCompletionPolicy.h"
#include "SessionManager.h"
#include "WifiOutageAggregator.h"

namespace {


void wifiNoApLogAggregationTest() {
  WifiOutageAggregator outage;
  assert(!outage.active());

  // Only the first NO_AP_FOUND should become a full trace event.
  assert(outage.recordNoAp(1000));
  assert(!outage.recordNoAp(4000));
  assert(!outage.recordNoAp(7000));
  assert(outage.active());
  assert(outage.totalAttempts() == 3);

  assert(!outage.periodicDue(60999, 60000));
  assert(outage.periodicDue(61000, 60000));
  WifiOutageAggregate periodic = outage.periodicSummary(61000);
  assert(periodic.durationMs == 60000);
  assert(periodic.totalNoApAttempts == 3);

  assert(!outage.recordNoAp(64000));
  assert(!outage.periodicDue(120999, 60000));
  assert(outage.periodicDue(121000, 60000));

  WifiOutageAggregate recovered = outage.finish(130000);
  assert(recovered.durationMs == 129000);
  assert(recovered.totalNoApAttempts == 4);
  assert(!outage.active());

  // A later outage starts a fresh aggregate.
  assert(outage.recordNoAp(200000));
  assert(outage.totalAttempts() == 1);
}

void exactMinuteTransitionTest() {
  SessionManager sessions;
  DeliveryManager deliveries;
  sessions.begin();
  deliveries.begin();

  assert(sessions.onValidBell(0, true, 1000, "seq-first") ==
         BellSessionResult::StartedFirst);
  SessionStatus status = sessions.status();
  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   status.firstSequenceId,
                                   1,
                                   true,
                                   1000,
                                   true,
                                   0));
  DeliveryAttempt attempt{};
  assert(deliveries.nextDue(0, attempt));
  deliveries.recordSuccess(attempt, 1000);

  for (int i = 0; i < 4; ++i) {
    assert(sessions.onValidBell(2000 + i * 1000,
                                true,
                                1002 + i,
                                "unused") ==
           BellSessionResult::UpdatedFirst);
  }
  status = sessions.status();
  deliveries.updateSequence(status.firstSequenceId,
                            status.totalPresses,
                            true,
                            1005,
                            55000);
  deliveries.forceSequence(status.firstSequenceId,
                           status.totalPresses,
                           true,
                           1005,
                           55000);
  assert(deliveries.nextDue(55000, attempt));
  assert(attempt.requestCount == 5);
}

void restoredSessionTimingTest() {
  SessionManager original;
  original.begin();
  original.onValidBell(1000, true, 1000, "restored-first");
  original.onValidBell(51000, true, 1050, "unused");
  const PersistentSessionState persisted = original.persistentState();

  SessionManager restored;
  // The previous first minute ended before this boot even started.
  assert(restored.restore(persisted, 5000, true, 1070));
  assert(restored.status().phase == SessionPhase::FirstFinalPending);
  assert(restored.status().firstWindowEndsMs <= 5000);
  assert(restored.status().firstPressMs <= 5000);
  assert(restored.status().lastPressMs <= 5000);
  assert(restored.status().sessionEndsMs == 105000);
  assert(restored.onValidBell(6000, true, 1071, "restored-insistence") ==
         BellSessionResult::StartedInsistence);
  assert(restored.status().totalPresses == 3);

  // An unexpired window keeps only its actual remaining time after reboot.
  assert(restored.restore(persisted, 5000, true, 1055));
  assert(restored.status().phase == SessionPhase::FirstMinute);
  assert(restored.status().firstWindowEndsMs == 10000);
  restored.tick(9999);
  assert(restored.status().phase == SessionPhase::FirstMinute);
  restored.tick(10000);
  assert(restored.status().phase == SessionPhase::FirstFinalPending);

  // Silence expiration and missing/future timestamps remain rejected.
  assert(!restored.restore(persisted, 5000, true, 1170));
  assert(!restored.restore(persisted, 5000, false, 1070));
  assert(!restored.restore(persisted, 5000, true, 1049));
  assert(restored.restore(persisted, 5000, true, 1169));
  restored.tick(5999);
  assert(restored.status().active);
  restored.tick(6000);
  assert(!restored.status().active);
}

void detectorQueuedValidPulsesTest() {
  // Exercise both the original six-pulse failure and the raw queue limit,
  // including an accumulated batch spanning micros() rollover.
  for (uint32_t pulseCount : {6U, 31U}) {
    for (uint32_t firstEdgeMs : {7100U, 4294900U}) {
      HostArduino::setLevel(LOW);
      HostArduino::setTime(0, 0);
      BellDetector detector;
      detector.begin();
      HostArduino::setTime(5000, 5000000);
      detector.tick();
      HostArduino::setTime(7000, 7000000);
      detector.tick();
      BellEvent event{};
      while (detector.popEvent(event)) {}

      for (uint32_t i = 0; i < pulseCount; ++i) {
        const uint32_t riseMs = firstEdgeMs + i * 180U;
        HostArduino::setTime(riseMs, riseMs * 1000U);
        HostArduino::setLevel(HIGH);
        HostArduino::triggerInterrupt();
        HostArduino::setTime(riseMs + 80U, (riseMs + 80U) * 1000U);
        HostArduino::setLevel(LOW);
        HostArduino::triggerInterrupt();
      }
      detector.tick();

      uint32_t delivered = 0;
      while (detector.popEvent(event)) {
        if (event.type == BellEventType::ValidPress) {
          ++delivered;
          assert(event.value == delivered);
          assert(event.pulseDurationUs == 80000U);
        }
      }
      assert(detector.status().stats.rawEdgesDropped == 0);
      assert(detector.status().stats.validPresses == pulseCount);
      assert(delivered == pulseCount);
      assert(detector.status().stats.detectorEventsDropped == 0);
    }
  }
}

void connectivityOfflinePolicyTest() {
  ConnectivityManager connectivity;
  connectivity.begin(0);
  connectivity.updateEnvironment(true, true, 0);
  assert(connectivity.status().state == ConnectivityState::Online);

  connectivity.recordPublishFailure(1000);
  assert(connectivity.status().state == ConnectivityState::Degraded);
  connectivity.recordPublishFailure(4000);
  connectivity.recordPublishFailure(11000);
  connectivity.recordPublishFailure(26000);
  assert(connectivity.status().state == ConnectivityState::Offline);
  assert(connectivity.status().offlineReason == OfflineReason::PublishFailures);
  assert(connectivity.consumeBecameUnavailable());

  connectivity.beginRecovery(70000);
  assert(connectivity.status().state == ConnectivityState::Recovering);
  connectivity.recordPublishSuccess(71000);
  assert(connectivity.status().state == ConnectivityState::Online);
  assert(connectivity.consumeRecovered());
}

void initialBootDoesNotCreateFalseOfflineSummaryTest() {
  ConnectivityManager connectivity;
  connectivity.begin(0);

  // Association + DHCP may take more than the normal five-second loss grace.
  connectivity.updateEnvironment(false, false, 1000);
  connectivity.updateEnvironment(false, false, 7000);
  connectivity.updateEnvironment(false, false, 15000);
  assert(connectivity.status().state == ConnectivityState::WaitingForTime);
  assert(!connectivity.consumeBecameUnavailable());

  connectivity.updateEnvironment(true, true, 16000);
  assert(connectivity.status().state == ConnectivityState::Online);

  // Once ONLINE was established, a real Wi-Fi loss still becomes OFFLINE
  // after the configured confirmation period.
  connectivity.updateEnvironment(false, true, 17000);
  connectivity.updateEnvironment(false, true,
      17000 + AppConfig::kWifiOfflineConfirmationMs);
  assert(connectivity.status().state == ConnectivityState::Offline);
  assert(connectivity.status().offlineReason == OfflineReason::WifiLost);
}

void wifiLossOfflineAfterFiveSecondsTest() {
  ConnectivityManager connectivity;
  connectivity.begin(0);
  connectivity.updateEnvironment(true, true, 1);
  connectivity.updateEnvironment(false, true, 1000);
  connectivity.updateEnvironment(false, true,
      1000 + AppConfig::kWifiOfflineConfirmationMs - 1);
  assert(connectivity.status().state != ConnectivityState::Offline);
  connectivity.updateEnvironment(false, true,
      1000 + AppConfig::kWifiOfflineConfirmationMs);
  assert(connectivity.status().state == ConnectivityState::Offline);
  assert(connectivity.status().offlineReason == OfflineReason::WifiLost);
}

void waitingForTimeBecomesOfflineTest() {
  ConnectivityManager connectivity;
  connectivity.begin(1);
  connectivity.updateEnvironment(true, false, 1);
  connectivity.updateEnvironment(true, false, 60000);
  assert(connectivity.status().state == ConnectivityState::WaitingForTime);
  connectivity.updateEnvironment(true, false, 60001);
  assert(connectivity.status().state == ConnectivityState::Offline);
  assert(connectivity.status().offlineReason == OfflineReason::TimeUnavailable);
}

void collapseOnlyUnreportedPressesTest() {
  DeliveryManager deliveries;
  deliveries.begin();

  // Occasion A: the phone already received 3 presses, then two more arrived.
  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   "a", 3, true, 100, true, 0));
  DeliveryAttempt attempt{};
  assert(deliveries.nextDue(0, attempt));
  assert(attempt.requestCount == 3);
  deliveries.recordSuccess(attempt, 1000);
  assert(deliveries.updateSequence("a", 5, true, 150, 2000));

  // Occasion B was never delivered.
  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   "b", 3, true, 200, true, 0));

  const DeliveryAggregate aggregate = deliveries.collapseUnreported();
  assert(aggregate.totalPresses == 5);  // 2 from A + all 3 from B.
  assert(aggregate.occasionCount == 2);
  assert(aggregate.latestEpochValid);
  assert(aggregate.latestEpoch == 200);
  assert(!deliveries.hasPendingJobs());
}

void fullyDeliveredOccasionExcludedFromOfflineSummaryTest() {
  DeliveryManager deliveries;
  deliveries.begin();
  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   "delivered", 3, true, 100, true, 0));
  DeliveryAttempt attempt{};
  assert(deliveries.nextDue(0, attempt));
  deliveries.recordSuccess(attempt, 1000);

  const DeliveryAggregate aggregate = deliveries.collapseUnreported();
  assert(aggregate.totalPresses == 0);
  assert(aggregate.occasionCount == 0);
  assert(!aggregate.latestEpochValid);
}

void threeDeliveredThenOneOfflineRegressionTest() {
  DeliveryManager deliveries;
  deliveries.begin();
  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   "same-occasion", 3, true, 100, true, 0));
  DeliveryAttempt attempt{};
  assert(deliveries.nextDue(0, attempt));
  deliveries.recordSuccess(attempt, 1000);

  assert(deliveries.updateSequence("same-occasion", 4, true, 110, 2000));
  const DeliveryAggregate aggregate = deliveries.collapseUnreported();
  assert(aggregate.totalPresses == 1);
  assert(aggregate.occasionCount == 1);
  assert(aggregate.latestEpochValid);
  assert(aggregate.latestEpoch == 110);
}

void insistenceInheritsPreviouslyDeliveredBaselineTest() {
  DeliveryManager deliveries;
  deliveries.begin();

  assert(deliveries.ensureSequence(NotificationStage::FirstMinute,
                                   "first", 3, true, 100, true, 0));
  DeliveryAttempt firstAttempt{};
  assert(deliveries.nextDue(0, firstAttempt));
  deliveries.recordSuccess(firstAttempt, 1000);

  uint32_t confirmedThrough = 0;
  assert(deliveries.supersedeSequence("first", &confirmedThrough));
  assert(confirmedThrough == 3);

  // The fourth press starts the insistence stage. The new sequence must still
  // be an audible first publish, but only one press is unreported if it fails.
  assert(deliveries.ensureSequence(NotificationStage::Insistence,
                                   "insistence", 4, true, 110, true, 2000,
                                   confirmedThrough));
  DeliveryAttempt insistenceAttempt{};
  assert(deliveries.nextDue(2000, insistenceAttempt));
  assert(insistenceAttempt.firstSuccessfulPublish);
  assert(insistenceAttempt.requestCount == 4);

  const DeliveryAggregate aggregate = deliveries.collapseUnreported();
  assert(aggregate.totalPresses == 1);
  assert(aggregate.occasionCount == 1);
}

void offlineOccasionGroupingTest() {
  OfflineSummaryManager summary;
  summary.begin(1234);
  summary.activate("recovery-sequence");
  summary.addHistorical(5, 1, true, 1000);
  summary.recordBell(10000, true, 1010);
  summary.recordBell(20000, true, 1020);
  OfflineSummaryStatus status = summary.status();
  assert(status.totalPresses == 7);
  assert(status.occasionCount == 2);  // Active online occasion + offline one.

  summary.tick(20000 + AppConfig::kOfflineOccasionSilenceMs);
  summary.recordBell(20000 + AppConfig::kOfflineOccasionSilenceMs + 1,
                     true,
                     1200);
  status = summary.status();
  assert(status.totalPresses == 8);
  assert(status.occasionCount == 3);
}

void relativeTimeSurvivesSameBootOnlyTest() {
  OfflineSummaryManager summary;
  summary.begin(111);
  summary.activate("relative");
  summary.recordBell(5000, false, 0);
  uint64_t age = 0;
  assert(summary.lastBellAgeSeconds(65000, 0, age));
  assert(age == 60);

  const PersistentOfflineSummary persisted = summary.persistentState();
  OfflineSummaryManager restored;
  restored.restore(persisted, 222);
  assert(restored.status().lastBellTimeQuality ==
         LastBellTimeQuality::UnknownAfterRestart);
  assert(!restored.lastBellAgeSeconds(1000, 0, age));
}



void detectorOverflowDoesNotCreateFalseLongPulseTest() {
  HostArduino::setLevel(LOW);
  HostArduino::setTime(0, 0);
  BellDetector detector;
  detector.begin();

  HostArduino::setTime(AppConfig::kBootWaitMs,
                       AppConfig::kBootWaitMs * 1000UL);
  detector.tick();
  HostArduino::setTime(AppConfig::kBootWaitMs + AppConfig::kArmLowStableMs,
                       (AppConfig::kBootWaitMs +
                        AppConfig::kArmLowStableMs) * 1000UL);
  detector.tick();

  BellEvent event{};
  while (detector.popEvent(event)) {
  }
  assert(detector.status().state == BellDetectorState::Armed);

  // Begin a normal HIGH, then generate more transitions than the raw queue can
  // hold. Timing after the overflow is deliberately considered unreliable.
  HostArduino::setLevel(HIGH);
  HostArduino::setTime(7100, 7100000);
  HostArduino::triggerInterrupt();
  detector.tick();
  while (detector.popEvent(event)) {
  }

  for (uint32_t i = 0; i < 101; ++i) {
    HostArduino::setLevel(HostArduino::digitalLevel == HIGH ? LOW : HIGH);
    HostArduino::setTime(7100, 7100001U + i);
    HostArduino::triggerInterrupt();
  }
  // Ensure the physical line is resting after the artificial bounce storm.
  HostArduino::setLevel(LOW);
  HostArduino::setTime(7101, 7100200);
  detector.tick();

  bool sawOverflow = false;
  bool sawFalseLong = false;
  while (detector.popEvent(event)) {
    sawOverflow = sawOverflow || event.type == BellEventType::RawQueueOverflow;
    sawFalseLong = sawFalseLong || event.type == BellEventType::HighTooLong;
  }
  assert(sawOverflow);
  assert(!sawFalseLong);

  HostArduino::setTime(7101 + AppConfig::kRearmLowStableMs,
                       7100200 + AppConfig::kRearmLowStableMs * 1000UL);
  detector.tick();
  assert(detector.status().state == BellDetectorState::Armed);
}

void recoveryCompletionWaitsForInFlightInputTest() {
  assert(RecoveryCompletionPolicy::afterSuccessfulPublish(true, false) ==
         RecoveryCompletionAction::WaitForInput);
  assert(RecoveryCompletionPolicy::afterSuccessfulPublish(true, true) ==
         RecoveryCompletionAction::WaitForInput);

  // A valid bell that finishes after HTTP 200 changes the summary and requires
  // a quiet update of the same recovery notification after the input settles.
  assert(RecoveryCompletionPolicy::afterInputSettles(true) ==
         RecoveryCompletionAction::ScheduleUpdate);

  // A short/rejected pulse changes nothing. Once the input settles, the
  // already-acknowledged recovery notification can be finalized without a
  // duplicate publish.
  assert(RecoveryCompletionPolicy::afterInputSettles(false) ==
         RecoveryCompletionAction::Finalize);
}

void recoveryCompletionImmediatePathsTest() {
  assert(RecoveryCompletionPolicy::afterSuccessfulPublish(false, false) ==
         RecoveryCompletionAction::Finalize);
  assert(RecoveryCompletionPolicy::afterSuccessfulPublish(false, true) ==
         RecoveryCompletionAction::ScheduleUpdate);
}

}  // namespace

int main() {
  wifiNoApLogAggregationTest();
  exactMinuteTransitionTest();
  restoredSessionTimingTest();
  detectorQueuedValidPulsesTest();
  connectivityOfflinePolicyTest();
  initialBootDoesNotCreateFalseOfflineSummaryTest();
  wifiLossOfflineAfterFiveSecondsTest();
  waitingForTimeBecomesOfflineTest();
  collapseOnlyUnreportedPressesTest();
  fullyDeliveredOccasionExcludedFromOfflineSummaryTest();
  threeDeliveredThenOneOfflineRegressionTest();
  insistenceInheritsPreviouslyDeliveredBaselineTest();
  offlineOccasionGroupingTest();
  relativeTimeSurvivesSameBootOnlyTest();
  detectorOverflowDoesNotCreateFalseLongPulseTest();
  recoveryCompletionWaitsForInFlightInputTest();
  recoveryCompletionImmediatePathsTest();
  std::cout << "Doorbell logic tests passed.\n";
  return 0;
}
