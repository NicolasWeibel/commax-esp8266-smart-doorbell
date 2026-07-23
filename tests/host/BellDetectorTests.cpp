#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "BellDetector.h"
#include "FakeArduino.h"

namespace {

struct TestFailure {
  std::string message;
};

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw TestFailure{message};
  }
}

std::vector<BellEvent> drainEvents(BellDetector& detector) {
  std::vector<BellEvent> events;
  BellEvent event{};
  while (detector.popEvent(event)) {
    events.push_back(event);
  }
  return events;
}

bool containsEvent(const std::vector<BellEvent>& events,
                   BellEventType type) {
  for (const BellEvent& event : events) {
    if (event.type == type) {
      return true;
    }
  }
  return false;
}

BellEvent findEvent(const std::vector<BellEvent>& events,
                    BellEventType type) {
  for (const BellEvent& event : events) {
    if (event.type == type) {
      return event;
    }
  }
  throw TestFailure{"Expected event was not emitted."};
}

void tickAfter(BellDetector& detector, uint32_t milliseconds) {
  FakeArduino::advanceMs(milliseconds);
  detector.tick();
}

void setD5AndTick(BellDetector& detector, bool high) {
  FakeArduino::setD5(high);
  detector.tick();
}

void armFromLow(BellDetector& detector) {
  tickAfter(detector, 5000);
  tickAfter(detector, 2000);
  drainEvents(detector);
  require(detector.status().state == BellDetectorState::Armed,
          "Detector did not arm after startup and stable LOW.");
}

void produceValidPulse(BellDetector& detector, uint32_t durationMs) {
  setD5AndTick(detector, true);
  tickAfter(detector, durationMs);
  setD5AndTick(detector, false);
}

void testBootAndArmFromLow() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();

  tickAfter(detector, 4999);
  require(detector.status().state == BellDetectorState::BootWait,
          "Detector left boot wait too early.");

  tickAfter(detector, 1);
  require(detector.status().state ==
              BellDetectorState::WaitingForStableLow,
          "Detector did not enter stable-LOW wait after five seconds.");

  tickAfter(detector, 1999);
  require(detector.status().state ==
              BellDetectorState::WaitingForStableLow,
          "Detector armed before two full seconds of LOW.");

  tickAfter(detector, 1);
  require(detector.status().state == BellDetectorState::Armed,
          "Detector did not arm after two seconds of LOW.");

  const auto events = drainEvents(detector);
  require(containsEvent(events, BellEventType::BootWaitFinished),
          "BootWaitFinished event missing.");
  require(containsEvent(events, BellEventType::Armed),
          "Armed event missing.");
}

void testValidPulseAnd100MsRearm() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  produceValidPulse(detector, 1000);

  const auto events = drainEvents(detector);
  const BellEvent valid = findEvent(events, BellEventType::ValidPress);
  require(valid.pulseDurationUs == 1000000U,
          "Valid pulse duration was not preserved in microseconds.");
  require(valid.value == 1, "Valid pulse total should be one.");
  require(detector.status().stats.validPresses == 1,
          "Valid pulse counter was not incremented.");
  require(detector.status().state == BellDetectorState::WaitingForRearm,
          "Detector did not enter rearm wait after a valid pulse.");

  tickAfter(detector, 99);
  require(detector.status().state == BellDetectorState::WaitingForRearm,
          "Detector rearmed before 100 ms of stable LOW.");

  tickAfter(detector, 1);
  require(detector.status().state == BellDetectorState::Armed,
          "Detector did not rearm after 100 ms of stable LOW.");
}

void testSubMillisecondShortPulsePreserved() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  setD5AndTick(detector, true);
  FakeArduino::advanceUs(437);
  setD5AndTick(detector, false);

  const auto events = drainEvents(detector);
  const BellEvent shortEvent =
      findEvent(events, BellEventType::ShortPulseIgnored);
  require(shortEvent.pulseDurationUs == 437U,
          "Sub-millisecond pulse duration was rounded or lost.");
  require(detector.status().stats.validPresses == 0,
          "Short pulse was incorrectly counted as valid.");
  require(detector.status().stats.shortPulsesIgnored == 1,
          "Short pulse counter was not incremented.");
}

void testLongHighIsNotBell() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  setD5AndTick(detector, true);
  tickAfter(detector, 15001);

  auto events = drainEvents(detector);
  require(containsEvent(events, BellEventType::HighTooLong),
          "Long HIGH event was not reported.");
  require(detector.status().state == BellDetectorState::HighTooLong,
          "Detector did not enter HighTooLong state.");
  require(detector.status().stats.validPresses == 0,
          "Long HIGH was incorrectly counted as a valid bell.");

  setD5AndTick(detector, false);
  events = drainEvents(detector);
  require(containsEvent(events, BellEventType::RecoveredFromHigh),
          "Recovery from long HIGH was not reported.");
  require(detector.status().state == BellDetectorState::WaitingForRearm,
          "Detector did not wait for rearm after long HIGH recovery.");
}

void testHighBefore100MsIsIgnoredAsNewBell() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  produceValidPulse(detector, 500);
  drainEvents(detector);
  require(detector.status().stats.validPresses == 1,
          "First valid pulse was not counted.");

  tickAfter(detector, 50);
  setD5AndTick(detector, true);
  require(detector.status().state == BellDetectorState::HighDuringRearm,
          "Early HIGH was not tracked as HIGH_DURING_REARM.");

  tickAfter(detector, 1000);
  setD5AndTick(detector, false);

  require(detector.status().stats.validPresses == 1,
          "HIGH before the 100 ms rearm boundary was counted as a new bell.");
  tickAfter(detector, 100);
  require(detector.status().state == BellDetectorState::Armed,
          "Detector did not rearm after the interrupted LOW interval restarted.");
}

void testFastSecondPressAfter100MsIsCounted() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  produceValidPulse(detector, 400);
  drainEvents(detector);
  require(detector.status().stats.validPresses == 1,
          "First valid pulse was not counted.");

  // Do not tick at the exact rearm boundary. The rising-edge timestamp itself
  // must prove that 100 ms of LOW elapsed and start the second pulse.
  FakeArduino::advanceMs(120);
  setD5AndTick(detector, true);
  tickAfter(detector, 350);
  setD5AndTick(detector, false);

  const auto events = drainEvents(detector);
  require(containsEvent(events, BellEventType::Armed),
          "Detector did not logically rearm before the fast second press.");
  require(containsEvent(events, BellEventType::ValidPress),
          "Fast second press after 100 ms was not emitted.");
  require(detector.status().stats.validPresses == 2,
          "Fast second press after 100 ms was not counted.");
}

void testLongHighBeginningDuringRearmIsReported() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  produceValidPulse(detector, 500);
  drainEvents(detector);

  tickAfter(detector, 50);
  setD5AndTick(detector, true);
  require(detector.status().state == BellDetectorState::HighDuringRearm,
          "Early HIGH did not enter HIGH_DURING_REARM.");

  tickAfter(detector, 15001);
  auto events = drainEvents(detector);
  require(containsEvent(events, BellEventType::HighTooLong),
          "Long HIGH beginning during rearm was not reported.");
  require(detector.status().state == BellDetectorState::HighTooLong,
          "Long HIGH during rearm did not enter HighTooLong state.");
  require(detector.status().stats.validPresses == 1,
          "Long HIGH during rearm was incorrectly counted as a bell.");

  setD5AndTick(detector, false);
  events = drainEvents(detector);
  require(containsEvent(events, BellEventType::RecoveredFromHigh),
          "Recovery from rearm-originated long HIGH was not reported.");
}

void testQueuedLongHighDuringRearmIsReported() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  produceValidPulse(detector, 500);
  drainEvents(detector);

  // Both edges occur while the main loop is notionally busy. The detector must
  // still classify the duration from queued ISR timestamps.
  FakeArduino::advanceMs(50);
  FakeArduino::setD5(true);
  FakeArduino::advanceMs(16000);
  FakeArduino::setD5(false);
  detector.tick();

  const auto events = drainEvents(detector);
  require(containsEvent(events, BellEventType::HighTooLong),
          "Queued long HIGH during rearm was not reported.");
  require(containsEvent(events, BellEventType::RecoveredFromHigh),
          "Queued long HIGH recovery event was not emitted.");
  require(detector.status().stats.highTooLongEvents == 1,
          "Queued long HIGH during rearm was not counted exactly once.");
  require(detector.status().stats.validPresses == 1,
          "Queued long HIGH during rearm was incorrectly counted as a bell.");
}

void testBootHighDoesNotCreateFalseBell() {
  FakeArduino::reset(true);
  BellDetector detector;
  detector.begin();

  tickAfter(detector, 5000);
  tickAfter(detector, 5000);
  require(detector.status().state ==
              BellDetectorState::WaitingForStableLow,
          "Detector armed while D5 remained HIGH after boot.");
  require(detector.status().stats.validPresses == 0,
          "Startup HIGH created a false bell event.");

  setD5AndTick(detector, false);
  tickAfter(detector, 1999);
  require(detector.status().state ==
              BellDetectorState::WaitingForStableLow,
          "Detector armed before two seconds after startup HIGH recovery.");
  tickAfter(detector, 1);
  require(detector.status().state == BellDetectorState::Armed,
          "Detector did not arm after startup HIGH returned to stable LOW.");
  require(detector.status().stats.validPresses == 0,
          "Returning to LOW after startup was incorrectly counted as a bell.");
}

void testQueuedEdgesPreservePulseDuration() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  // Simulate a future blocking network operation: both edges are captured by
  // the ISR, but the main loop does not process them until several seconds later.
  FakeArduino::setD5(true);
  FakeArduino::advanceMs(1000);
  FakeArduino::setD5(false);
  FakeArduino::advanceMs(4000);
  detector.tick();

  const auto events = drainEvents(detector);
  const BellEvent valid = findEvent(events, BellEventType::ValidPress);
  require(valid.pulseDurationUs == 1000000U,
          "Queued edges lost the original pulse duration.");
  require(detector.status().stats.validPresses == 1,
          "Queued valid pulse was not counted.");
}

void testMicrosRolloverDuringPulse() {
  FakeArduino::reset(false);
  BellDetector detector;
  detector.begin();
  armFromLow(detector);

  // Move close to the 32-bit micros() rollover, then cross it during a pulse.
  FakeArduino::setTimeUs((1ULL << 32U) - 500000ULL);
  FakeArduino::setD5(true);
  detector.tick();
  FakeArduino::advanceMs(1000);
  FakeArduino::setD5(false);
  detector.tick();

  const auto events = drainEvents(detector);
  const BellEvent valid = findEvent(events, BellEventType::ValidPress);
  require(valid.pulseDurationUs == 1000000U,
          "Pulse duration failed across micros() rollover.");
  require(detector.status().stats.validPresses == 1,
          "Rollover pulse was not counted as valid.");
}

template <typename Function>
bool runTest(const char* name, Function function) {
  try {
    function();
    std::cout << "[PASS] " << name << '\n';
    return true;
  } catch (const TestFailure& failure) {
    std::cerr << "[FAIL] " << name << ": " << failure.message << '\n';
    return false;
  }
}

}  // namespace

int main() {
  bool passed = true;
  passed &= runTest("boot and arm from LOW", testBootAndArmFromLow);
  passed &= runTest("valid pulse and 100 ms rearm",
                    testValidPulseAnd100MsRearm);
  passed &= runTest("sub-millisecond short pulse preserved",
                    testSubMillisecondShortPulsePreserved);
  passed &= runTest("long HIGH is not a bell", testLongHighIsNotBell);
  passed &= runTest("HIGH before 100 ms is ignored as a new bell",
                    testHighBefore100MsIsIgnoredAsNewBell);
  passed &= runTest("fast second press after 100 ms is counted",
                    testFastSecondPressAfter100MsIsCounted);
  passed &= runTest("long HIGH beginning during rearm is reported",
                    testLongHighBeginningDuringRearmIsReported);
  passed &= runTest("queued long HIGH during rearm is reported",
                    testQueuedLongHighDuringRearmIsReported);
  passed &= runTest("boot HIGH does not create false bell",
                    testBootHighDoesNotCreateFalseBell);
  passed &= runTest("queued edges preserve pulse duration",
                    testQueuedEdgesPreservePulseDuration);
  passed &= runTest("micros rollover during pulse",
                    testMicrosRolloverDuringPulse);

  if (!passed) {
    return EXIT_FAILURE;
  }

  std::cout << "All BellDetector host tests passed.\n";
  return EXIT_SUCCESS;
}
