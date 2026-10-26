#include "TimeKeeper.h"

#include "AppConfig.h"
#include "Secrets.h"

void TimeKeeper::begin(uint64_t nowMs) {
  setenv("TZ", Secrets::kTimeZone, 1);
  tzset();
  monotonicAnchorMs_ = nowMs;
  lastPollMs_ = 0;
}

void TimeKeeper::tick(uint64_t nowMs, bool wifiConnected) {
  if (wifiConnected && !configured_) {
    // Use the POSIX timezone overload. The previous numeric overload with
    // zero offsets reset TZ to UTC after begin(), so notifications showed UTC.
    configTime(Secrets::kTimeZone,
               "pool.ntp.org",
               "time.cloudflare.com",
               "time.nist.gov");
    configured_ = true;
  }

  if ((nowMs - lastPollMs_) < AppConfig::kNtpPollIntervalMs) {
    return;
  }
  lastPollMs_ = nowMs;

  const time_t current = time(nullptr);
  if (current >= AppConfig::kMinimumValidEpoch) {
    const bool wasValid = valid_;
    valid_ = true;
    epochAnchor_ = current;
    monotonicAnchorMs_ = nowMs;
    if (!wasValid) {
      newSynchronization_ = true;
    }
  }
}

bool TimeKeeper::valid() const {
  return valid_;
}

time_t TimeKeeper::nowEpoch(uint64_t nowMs) const {
  if (!valid_) {
    return 0;
  }
  return epochAnchor_ +
         static_cast<time_t>((nowMs - monotonicAnchorMs_) / 1000ULL);
}

bool TimeKeeper::reconstructEpoch(uint64_t eventMonotonicMs,
                                  uint64_t nowMonotonicMs,
                                  time_t& epochOut) const {
  if (!valid_ || eventMonotonicMs > nowMonotonicMs) {
    return false;
  }

  const uint64_t ageSeconds =
      (nowMonotonicMs - eventMonotonicMs) / 1000ULL;
  const time_t current = nowEpoch(nowMonotonicMs);
  if (current <= static_cast<time_t>(ageSeconds)) {
    return false;
  }

  epochOut = current - static_cast<time_t>(ageSeconds);
  return true;
}

bool TimeKeeper::formatLocal(time_t epoch,
                             char* output,
                             size_t outputSize) const {
  if (output == nullptr || outputSize == 0 || epoch <= 0) {
    return false;
  }

  struct tm localTime{};
  if (localtime_r(&epoch, &localTime) == nullptr) {
    return false;
  }

  return strftime(output, outputSize, "%Y-%m-%d %H:%M", &localTime) > 0;
}

bool TimeKeeper::formatLocalTime(time_t epoch,
                                 char* output,
                                 size_t outputSize) const {
  if (output == nullptr || outputSize == 0 || epoch <= 0) {
    return false;
  }

  struct tm localTime{};
  if (localtime_r(&epoch, &localTime) == nullptr) {
    return false;
  }

  return strftime(output, outputSize, "%H:%M", &localTime) > 0;
}

bool TimeKeeper::consumeNewSynchronization() {
  const bool result = newSynchronization_;
  newSynchronization_ = false;
  return result;
}
