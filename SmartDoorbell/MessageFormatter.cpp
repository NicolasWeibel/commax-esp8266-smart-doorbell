#include "MessageFormatter.h"

String MessageFormatter::title(NotificationStage stage) const {
  if (stage == NotificationStage::RecoverySummary) {
    return F("Portero nuevamente en línea");
  }
  return stage == NotificationStage::Insistence
             ? F("Insistieron con el timbre")
             : F("Timbre");
}

String MessageFormatter::message(NotificationStage stage,
                                 uint32_t count,
                                 bool lastPressEpochValid,
                                 time_t lastPressEpoch,
                                 time_t currentEpoch,
                                 const TimeKeeper& timeKeeper) const {
  String output;
  output.reserve(220);

  const String moment = formatNaturalMoment(lastPressEpochValid,
                                            lastPressEpoch,
                                            currentEpoch,
                                            timeKeeper);

  if (stage == NotificationStage::Insistence) {
    output += F("Insistieron con el timbre ");
    output += String(count);
    output += count == 1U ? F(" vez.") : F(" veces.");
    if (moment.length() > 0U) {
      output += F(" La última fue ");
      output += moment;
      output += '.';
    } else {
      output += F(" No se pudo determinar la hora del último toque.");
    }
    return output;
  }

  if (count <= 1U) {
    if (moment.length() > 0U) {
      output += F("Tocaron el timbre ");
      output += moment;
      output += '.';
    } else {
      output += F("Tocaron el timbre. No se pudo determinar la hora del último toque.");
    }
    return output;
  }

  output += F("Tocaron el timbre ");
  output += String(count);
  output += F(" veces en el último minuto.");
  if (moment.length() > 0U) {
    output += F(" La última fue ");
    output += moment;
    output += '.';
  } else {
    output += F(" No se pudo determinar la hora del último toque.");
  }
  return output;
}

String MessageFormatter::recoveryMessage(
    const OfflineSummaryStatus& summary,
    bool ageAvailable,
    uint64_t ageSeconds,
    time_t currentEpoch,
    const TimeKeeper& timeKeeper) const {
  String output;
  output.reserve(340);

  bool epochValid = false;
  time_t lastBellEpoch = 0;
  if (summary.lastBellTimeQuality == LastBellTimeQuality::ExactEpoch &&
      summary.lastBellEpoch > 0) {
    epochValid = true;
    lastBellEpoch = summary.lastBellEpoch;
  } else if (summary.lastBellTimeQuality == LastBellTimeQuality::RelativeUptime &&
             ageAvailable && currentEpoch > 0 &&
             ageSeconds < static_cast<uint64_t>(currentEpoch)) {
    // The existing summary manager validates that this age belongs to this
    // boot. Reconstruct only the displayed date, without changing saved state.
    epochValid = true;
    lastBellEpoch = currentEpoch - static_cast<time_t>(ageSeconds);
  }
  const String moment = formatNaturalMoment(epochValid,
                                            lastBellEpoch,
                                            currentEpoch,
                                            timeKeeper);

  if (summary.totalPresses <= 1U) {
    output += F("Mientras estuvo sin conexión, tocaron el timbre una vez");
    if (moment.length() > 0U) {
      output += ' ';
      output += moment;
      output += '.';
      return output;
    }
    output += '.';
  } else {
    output += F("Mientras estuvo sin conexión, tocaron el timbre ");
    output += String(summary.totalPresses);
    if (summary.occasionCount <= 1U) {
      output += F(" veces en una sola ocasión.");
    } else {
      output += F(" veces, repartidas en ");
      output += String(summary.occasionCount);
      output += F(" ocasiones distintas.");
    }
    if (moment.length() > 0U) {
      output += F(" La última fue ");
      output += moment;
      output += '.';
      return output;
    }
  }

  if (summary.lastBellTimeQuality == LastBellTimeQuality::UnknownAfterRestart) {
    output += F(" No se pudo determinar la hora del último toque porque el portero se reinició sin conexión.");
  } else {
    output += F(" No se pudo determinar la hora del último toque.");
  }
  return output;
}

String MessageFormatter::formatNaturalMoment(
    bool epochValid,
    time_t eventEpoch,
    time_t currentEpoch,
    const TimeKeeper& timeKeeper) const {
  if (!epochValid || eventEpoch <= 0) {
    return String();
  }

  char localClock[8]{};
  if (!timeKeeper.formatLocalTime(eventEpoch, localClock, sizeof(localClock))) {
    return String();
  }
  struct tm eventLocal{};
  if (localtime_r(&eventEpoch, &eventLocal) == nullptr) {
    return String();
  }
  struct tm currentLocal{};
  const bool currentDateAvailable =
      currentEpoch > 0 && localtime_r(&currentEpoch, &currentLocal) != nullptr;

  String result;
  result.reserve(48);
  if (!currentDateAvailable ||
      (eventLocal.tm_year == currentLocal.tm_year &&
       eventLocal.tm_yday == currentLocal.tm_yday)) {
    result += F("a las ");
    result += localClock;
    return result;
  }

  const bool yesterdaySameYear =
      eventLocal.tm_year == currentLocal.tm_year &&
      eventLocal.tm_yday + 1 == currentLocal.tm_yday;
  const bool yesterdayAcrossYear =
      eventLocal.tm_year + 1 == currentLocal.tm_year &&
      eventLocal.tm_mon == 11 && eventLocal.tm_mday == 31 &&
      currentLocal.tm_mon == 0 && currentLocal.tm_mday == 1;
  if (yesterdaySameYear || yesterdayAcrossYear) {
    result += F("ayer a las ");
    result += localClock;
    return result;
  }

  result += F("el ");
  result += String(eventLocal.tm_mday);
  result += F(" de ");
  result += monthName(static_cast<uint8_t>(eventLocal.tm_mon));
  if (eventLocal.tm_year != currentLocal.tm_year) {
    result += F(" de ");
    result += String(eventLocal.tm_year + 1900);
  }
  result += F(" a las ");
  result += localClock;
  return result;
}

const char* MessageFormatter::monthName(uint8_t month) const {
  static const char* const kMonths[] = {
      "enero", "febrero", "marzo", "abril", "mayo", "junio",
      "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre",
  };
  return month < 12U ? kMonths[month] : "";
}
