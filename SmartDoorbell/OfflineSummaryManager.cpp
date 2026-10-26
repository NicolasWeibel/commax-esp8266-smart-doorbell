#include "OfflineSummaryManager.h"

#include <cstring>

void OfflineSummaryManager::begin(uint32_t bootNonce) {
  bootNonce_ = bootNonce;
  status_ = OfflineSummaryStatus{};
}

void OfflineSummaryManager::restore(
    const PersistentOfflineSummary& persisted,
    uint32_t currentBootNonce) {
  begin(currentBootNonce);
  if (persisted.active == 0U) {
    return;
  }

  status_.active = true;
  status_.initialConfirmed = persisted.initialConfirmed != 0U;
  status_.totalPresses = persisted.totalPresses;
  status_.occasionCount = persisted.occasionCount;
  status_.lastBellTimeQuality =
      static_cast<LastBellTimeQuality>(persisted.lastBellTimeQuality);
  status_.lastBellEpoch = static_cast<time_t>(persisted.lastBellEpoch);
  status_.lastBellMonotonicMs = persisted.lastBellMonotonicMs;
  status_.lastBellBootNonce = persisted.lastBellBootNonce;
  std::strncpy(status_.sequenceId,
               persisted.sequenceId,
               sizeof(status_.sequenceId) - 1U);

  // An uptime timestamp is meaningful only inside the boot that created it.
  if (status_.lastBellTimeQuality == LastBellTimeQuality::RelativeUptime &&
      status_.lastBellBootNonce != currentBootNonce) {
    setLatestUnknown();
  }

  // A reboot always closes the previous human-visible occasion. A later bell
  // starts a new occasion even when it happens soon after boot.
  status_.currentOccasionOpen = false;
  status_.lastOccasionBellMs = 0;
}

void OfflineSummaryManager::activate(const char* sequenceId) {
  if (status_.active) {
    return;
  }
  status_ = OfflineSummaryStatus{};
  status_.active = true;
  if (sequenceId != nullptr) {
    std::strncpy(status_.sequenceId,
                 sequenceId,
                 sizeof(status_.sequenceId) - 1U);
  }
}

void OfflineSummaryManager::addHistorical(uint32_t presses,
                                          uint32_t occasions,
                                          bool latestEpochValid,
                                          time_t latestEpoch) {
  if (!status_.active || presses == 0U) {
    return;
  }
  status_.totalPresses += presses;
  status_.occasionCount += occasions > 0U ? occasions : 1U;
  if (latestEpochValid && latestEpoch > 0) {
    setLatestExact(latestEpoch);
  } else if (status_.lastBellTimeQuality == LastBellTimeQuality::None) {
    setLatestUnknown();
  }
  status_.currentOccasionOpen = false;
}

void OfflineSummaryManager::setLatestExact(time_t epoch) {
  if (!status_.active || epoch <= 0) {
    return;
  }
  status_.lastBellTimeQuality = LastBellTimeQuality::ExactEpoch;
  status_.lastBellEpoch = epoch;
  status_.lastBellMonotonicMs = 0;
  status_.lastBellBootNonce = 0;
}

void OfflineSummaryManager::setLatestRelative(uint64_t monotonicMs) {
  if (!status_.active) {
    return;
  }
  status_.lastBellTimeQuality = LastBellTimeQuality::RelativeUptime;
  status_.lastBellEpoch = 0;
  status_.lastBellMonotonicMs = monotonicMs;
  status_.lastBellBootNonce = bootNonce_;
}

void OfflineSummaryManager::setLatestUnknown() {
  if (!status_.active) {
    return;
  }
  status_.lastBellTimeQuality = LastBellTimeQuality::UnknownAfterRestart;
  status_.lastBellEpoch = 0;
  status_.lastBellMonotonicMs = 0;
  status_.lastBellBootNonce = 0;
}

void OfflineSummaryManager::recordBell(uint64_t nowMs,
                                       bool epochValid,
                                       time_t epoch) {
  if (!status_.active) {
    return;
  }

  const bool startsNewOccasion =
      !status_.currentOccasionOpen ||
      nowMs < status_.lastOccasionBellMs ||
      (nowMs - status_.lastOccasionBellMs) >=
          AppConfig::kOfflineOccasionSilenceMs;
  if (startsNewOccasion) {
    ++status_.occasionCount;
  }
  ++status_.totalPresses;
  status_.currentOccasionOpen = true;
  status_.lastOccasionBellMs = nowMs;

  if (epochValid && epoch > 0) {
    setLatestExact(epoch);
  } else {
    setLatestRelative(nowMs);
  }
}

void OfflineSummaryManager::tick(uint64_t nowMs) {
  if (!status_.active || !status_.currentOccasionOpen) {
    return;
  }
  if (nowMs < status_.lastOccasionBellMs ||
      (nowMs - status_.lastOccasionBellMs) >=
          AppConfig::kOfflineOccasionSilenceMs) {
    status_.currentOccasionOpen = false;
  }
}

void OfflineSummaryManager::markInitialConfirmed() {
  if (status_.active) {
    status_.initialConfirmed = true;
  }
}

void OfflineSummaryManager::clear() {
  status_ = OfflineSummaryStatus{};
}

bool OfflineSummaryManager::active() const {
  return status_.active;
}

bool OfflineSummaryManager::hasBells() const {
  return status_.active && status_.totalPresses > 0U;
}

OfflineSummaryStatus OfflineSummaryManager::status() const {
  return status_;
}

PersistentOfflineSummary OfflineSummaryManager::persistentState() const {
  PersistentOfflineSummary persisted{};
  persisted.active = status_.active ? 1U : 0U;
  persisted.initialConfirmed = status_.initialConfirmed ? 1U : 0U;
  persisted.lastBellTimeQuality =
      static_cast<uint8_t>(status_.lastBellTimeQuality);
  persisted.totalPresses = status_.totalPresses;
  persisted.occasionCount = status_.occasionCount;
  persisted.lastBellEpoch = static_cast<int64_t>(status_.lastBellEpoch);
  persisted.lastBellMonotonicMs = status_.lastBellMonotonicMs;
  persisted.lastBellBootNonce = status_.lastBellBootNonce;
  std::strncpy(persisted.sequenceId,
               status_.sequenceId,
               sizeof(persisted.sequenceId) - 1U);
  return persisted;
}

bool OfflineSummaryManager::lastBellAgeSeconds(
    uint64_t nowMs,
    time_t currentEpoch,
    uint64_t& ageSeconds) const {
  ageSeconds = 0;
  if (!hasBells()) {
    return false;
  }

  if (status_.lastBellTimeQuality == LastBellTimeQuality::ExactEpoch) {
    if (currentEpoch <= 0 || status_.lastBellEpoch <= 0 ||
        currentEpoch < status_.lastBellEpoch) {
      return false;
    }
    ageSeconds = static_cast<uint64_t>(currentEpoch - status_.lastBellEpoch);
    return true;
  }

  if (status_.lastBellTimeQuality == LastBellTimeQuality::RelativeUptime &&
      status_.lastBellBootNonce == bootNonce_ &&
      nowMs >= status_.lastBellMonotonicMs) {
    ageSeconds = (nowMs - status_.lastBellMonotonicMs) / 1000ULL;
    return true;
  }

  return false;
}
