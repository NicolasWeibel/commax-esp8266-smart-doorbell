#include <cassert>
#include <cstdlib>
#include <iostream>

#include "MessageFormatter.h"
#include "OfflineSummaryManager.h"

// The unchanged TimeKeeper HH:MM service is substituted for host tests; NTP
// and the ESP8266 clock are not exercised here. The formatter itself is real.
bool TimeKeeper::formatLocalTime(time_t epoch,
                                 char* output,
                                 size_t outputSize) const {
  if (output == nullptr || outputSize == 0 || epoch <= 0) {
    return false;
  }
  struct tm local{};
  return localtime_r(&epoch, &local) != nullptr &&
         strftime(output, outputSize, "%H:%M", &local) > 0;
}

namespace {

void useArgentinaTime() {
#ifdef _WIN32
  assert(_putenv_s("TZ", "ART3") == 0);
  _tzset();
#else
  assert(setenv("TZ", "ART3", 1) == 0);
  tzset();
#endif
}

time_t localEpoch(int year, int month, int day, int hour, int minute) {
  struct tm local{};
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = day;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_isdst = -1;
  const time_t result = mktime(&local);
  assert(result > 0);
  return result;
}

void normalMessagesUseLastPressRatherThanSendTime() {
  MessageFormatter formatter;
  TimeKeeper clock;
  const time_t sent = localEpoch(2026, 10, 19, 18, 5);
  const time_t first = localEpoch(2026, 10, 19, 16, 42);
  const time_t last = localEpoch(2026, 10, 19, 16, 43);
  assert(formatter.message(NotificationStage::FirstMinute, 1, true, first,
                           sent, clock) ==
         "Tocaron el timbre a las 16:42.");
  assert(formatter.message(NotificationStage::FirstMinute, 3, true, last,
                           sent, clock) ==
         "Tocaron el timbre 3 veces en el último minuto. La última fue a las 16:43.");
  assert(formatter.message(NotificationStage::Insistence, 4, true, last,
                           sent, clock) ==
         "Insistieron con el timbre 4 veces. La última fue a las 16:43.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, false, first,
                           sent, clock) ==
         "Tocaron el timbre. No se pudo determinar la hora del último toque.");
  assert(formatter.message(NotificationStage::Insistence, 1, true, 0,
                           sent, clock) ==
         "Insistieron con el timbre 1 vez. No se pudo determinar la hora del último toque.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, true, first,
                           0, clock) == "Tocaron el timbre a las 16:42.");
}

void calendarBoundariesUseLocalDates() {
  MessageFormatter formatter;
  TimeKeeper clock;
  // Both epochs are Jan 1 in UTC, but straddle midnight/year in Argentina.
  assert(formatter.message(NotificationStage::FirstMinute, 1, true,
                           1704076200, 1704081600, clock) ==
         "Tocaron el timbre ayer a las 23:30.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, true,
                           1704067200, 1704067500, clock) ==
         "Tocaron el timbre a las 21:00.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, true,
                           localEpoch(2024, 2, 29, 23, 59),
                           localEpoch(2024, 3, 1, 0, 1), clock) ==
         "Tocaron el timbre ayer a las 23:59.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, true,
                           localEpoch(2026, 9, 17, 18, 21),
                           localEpoch(2026, 10, 19, 16, 0), clock) ==
         "Tocaron el timbre el 17 de septiembre a las 18:21.");
  assert(formatter.message(NotificationStage::FirstMinute, 1, true,
                           localEpoch(2025, 9, 17, 18, 21),
                           localEpoch(2026, 10, 19, 16, 0), clock) ==
         "Tocaron el timbre el 17 de septiembre de 2025 a las 18:21.");
}

void recoveryPreservesCountsAndUsesStoredEpoch() {
  MessageFormatter formatter;
  TimeKeeper clock;
  OfflineSummaryStatus summary{};
  summary.totalPresses = 3;
  summary.occasionCount = 1;
  summary.lastBellTimeQuality = LastBellTimeQuality::ExactEpoch;
  summary.lastBellEpoch = localEpoch(2026, 10, 18, 22, 37);
  const time_t sent = localEpoch(2026, 10, 19, 16, 0);
  assert(formatter.recoveryMessage(summary, true, 62580, sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre 3 veces en una sola ocasión. La última fue ayer a las 22:37.");
  summary.totalPresses = 5;
  summary.occasionCount = 2;
  summary.lastBellEpoch = localEpoch(2026, 9, 17, 18, 21);
  assert(formatter.recoveryMessage(summary, false, 0, sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre 5 veces, repartidas en 2 ocasiones distintas. La última fue el 17 de septiembre a las 18:21.");
}

void relativeRecoveryRequiresSameBootAndValidAge() {
  MessageFormatter formatter;
  TimeKeeper clock;
  OfflineSummaryManager manager;
  manager.begin(77);
  manager.activate("recovery");
  manager.recordBell(5000, false, 0);
  const time_t sent = localEpoch(2026, 10, 19, 13, 35);
  uint64_t age = 0;
  assert(manager.lastBellAgeSeconds(125000, sent, age));
  assert(age == 120);
  const PersistentOfflineSummary before = manager.persistentState();
  assert(formatter.recoveryMessage(manager.status(), true, age, sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre una vez a las 13:33.");
  assert(manager.status().lastBellTimeQuality == LastBellTimeQuality::RelativeUptime);
  assert(manager.status().lastBellMonotonicMs == before.lastBellMonotonicMs);
  assert(formatter.recoveryMessage(manager.status(), false, 0, sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre una vez. No se pudo determinar la hora del último toque.");
  assert(formatter.recoveryMessage(manager.status(), true,
                                   static_cast<uint64_t>(sent), sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre una vez. No se pudo determinar la hora del último toque.");
  OfflineSummaryManager restarted;
  restarted.restore(before, 78);
  assert(!restarted.lastBellAgeSeconds(1000, sent, age));
  assert(formatter.recoveryMessage(restarted.status(), false, age, sent, clock) ==
         "Mientras estuvo sin conexión, tocaron el timbre una vez. No se pudo determinar la hora del último toque porque el portero se reinició sin conexión.");
}

}  // namespace

int main() {
  useArgentinaTime();
  normalMessagesUseLastPressRatherThanSendTime();
  calendarBoundariesUseLocalDates();
  recoveryPreservesCountsAndUsesStoredEpoch();
  relativeRecoveryRequiresSameBootAndValidAge();
  std::cout << "Natural-time message tests passed (ART3).\n";
}
