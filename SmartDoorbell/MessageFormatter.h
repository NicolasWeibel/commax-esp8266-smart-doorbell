#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "TimeKeeper.h"

class MessageFormatter {
 public:
  String title(NotificationStage stage) const;
  String message(NotificationStage stage,
                 uint32_t count,
                 bool lastPressEpochValid,
                 time_t lastPressEpoch,
                 time_t currentEpoch,
                 const TimeKeeper& timeKeeper) const;
  String recoveryMessage(const OfflineSummaryStatus& summary,
                         bool ageAvailable,
                         uint64_t ageSeconds,
                         time_t currentEpoch,
                         const TimeKeeper& timeKeeper) const;

 private:
  String formatNaturalMoment(bool epochValid,
                             time_t eventEpoch,
                             time_t currentEpoch,
                             const TimeKeeper& timeKeeper) const;
  const char* monthName(uint8_t month) const;
};
