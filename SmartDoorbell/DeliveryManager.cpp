#include "DeliveryManager.h"

#include <cstring>

void DeliveryManager::begin() {
  for (auto& job : jobs_) {
    job = Job{};
  }
  generationCounter_ = 0;
  successfulPublishes_ = 0;
  failedPublishes_ = 0;
  supersededJobs_ = 0;
  queueFullEvents_ = 0;
}

bool DeliveryManager::ensureSequence(NotificationStage stage,
                                     const char* sequenceId,
                                     uint32_t count,
                                     bool lastPressEpochValid,
                                     time_t lastPressEpoch,
                                     bool urgent,
                                     uint64_t nowMs,
                                     uint32_t confirmedBaseline) {
  if (sequenceId == nullptr || sequenceId[0] == '\0' || count == 0U) {
    return false;
  }

  int index = findBySequence(sequenceId);
  if (index < 0) {
    index = findFreeSlot();
    if (index < 0) {
      ++queueFullEvents_;
      return false;
    }

    Job& job = jobs_[index];
    job = Job{};
    job.used = true;
    job.stage = stage;
    job.urgent = urgent;
    job.generation = ++generationCounter_;
    job.nextAttemptMs = nowMs;
    std::strncpy(job.sequenceId,
                 sequenceId,
                 sizeof(job.sequenceId) - 1U);
  }

  Job& job = jobs_[index];
  job.stage = stage;
  job.urgent = urgent;
  if (count > job.desiredCount) {
    job.desiredCount = count;
  }
  const uint32_t boundedBaseline =
      confirmedBaseline < job.desiredCount
          ? confirmedBaseline
          : job.desiredCount;
  if (boundedBaseline > job.confirmedCount) {
    // A new human-visible stage (for example, insistence) may inherit the
    // number of presses already acknowledged by the previous stage. This
    // baseline prevents the offline summary from counting those presses again,
    // while initialConfirmed remains false so the new stage can still be
    // delivered as its first audible notification.
    job.confirmedCount = boundedBaseline;
  }
  job.lastPressEpochValid = lastPressEpochValid;
  job.lastPressEpoch = lastPressEpochValid ? lastPressEpoch : 0;
  refreshDueAfterDesiredChange(job, nowMs);
  return true;
}

bool DeliveryManager::updateSequence(const char* sequenceId,
                                     uint32_t count,
                                     bool lastPressEpochValid,
                                     time_t lastPressEpoch,
                                     uint64_t nowMs) {
  const int index = findBySequence(sequenceId);
  if (index < 0) {
    return false;
  }

  Job& job = jobs_[index];
  if (count > job.desiredCount) {
    job.desiredCount = count;
  }
  job.lastPressEpochValid = lastPressEpochValid;
  job.lastPressEpoch = lastPressEpochValid ? lastPressEpoch : 0;
  refreshDueAfterDesiredChange(job, nowMs);
  return true;
}

bool DeliveryManager::forceSequence(const char* sequenceId,
                                    uint32_t count,
                                    bool lastPressEpochValid,
                                    time_t lastPressEpoch,
                                    uint64_t nowMs) {
  const int index = findBySequence(sequenceId);
  if (index < 0) {
    return false;
  }

  Job& job = jobs_[index];
  if (count > job.desiredCount) {
    job.desiredCount = count;
  }
  job.lastPressEpochValid = lastPressEpochValid;
  job.lastPressEpoch = lastPressEpochValid ? lastPressEpoch : 0;
  if (!job.initialConfirmed || job.desiredCount > job.confirmedCount) {
    job.nextAttemptMs = nowMs;
  }
  return true;
}

bool DeliveryManager::closeSequence(const char* sequenceId,
                                    uint32_t count,
                                    bool lastPressEpochValid,
                                    time_t lastPressEpoch,
                                    uint64_t nowMs) {
  const int index = findBySequence(sequenceId);
  if (index < 0) {
    return false;
  }

  Job& job = jobs_[index];
  job.closedFinal = true;
  if (count > job.desiredCount) {
    job.desiredCount = count;
  }
  job.lastPressEpochValid = lastPressEpochValid;
  job.lastPressEpoch = lastPressEpochValid ? lastPressEpoch : 0;

  if (job.initialConfirmed && job.desiredCount <= job.confirmedCount) {
    removeSlot(static_cast<uint8_t>(index));
  } else {
    job.nextAttemptMs = nowMs;
  }
  return true;
}

bool DeliveryManager::supersedeSequence(const char* sequenceId,
                                        uint32_t* confirmedCount) {
  if (confirmedCount != nullptr) {
    *confirmedCount = 0U;
  }
  const int index = findBySequence(sequenceId);
  if (index < 0) {
    return false;
  }
  if (confirmedCount != nullptr) {
    *confirmedCount = jobs_[index].confirmedCount;
  }
  removeSlot(static_cast<uint8_t>(index));
  ++supersededJobs_;
  return true;
}

bool DeliveryManager::nextDue(uint64_t nowMs,
                              DeliveryAttempt& attempt) const {
  int best = -1;
  uint8_t bestPriority = 0;
  uint64_t bestDue = UINT64_MAX;

  for (uint8_t i = 0; i < kPersistentDeliveryJobCapacity; ++i) {
    const Job& job = jobs_[i];
    if (!job.used || job.desiredCount == 0U ||
        job.nextAttemptMs > nowMs) {
      continue;
    }

    const uint8_t score = priorityScore(job);
    if (best < 0 || score > bestPriority ||
        (score == bestPriority && job.nextAttemptMs < bestDue)) {
      best = i;
      bestPriority = score;
      bestDue = job.nextAttemptMs;
    }
  }

  if (best < 0) {
    attempt = DeliveryAttempt{};
    return false;
  }

  const Job& job = jobs_[best];
  attempt = DeliveryAttempt{};
  attempt.valid = true;
  attempt.slot = static_cast<uint8_t>(best);
  attempt.generation = job.generation;
  attempt.stage = job.stage;
  attempt.requestCount = job.desiredCount;
  attempt.lastPressEpochValid = job.lastPressEpochValid;
  attempt.lastPressEpoch = job.lastPressEpoch;
  attempt.firstSuccessfulPublish = !job.initialConfirmed;
  attempt.urgent = job.urgent;
  attempt.startedMs = nowMs;
  std::strncpy(attempt.sequenceId,
               job.sequenceId,
               sizeof(attempt.sequenceId) - 1U);
  return true;
}

void DeliveryManager::recordSuccess(const DeliveryAttempt& attempt,
                                    uint64_t nowMs) {
  if (!attempt.valid || attempt.slot >= kPersistentDeliveryJobCapacity) {
    return;
  }

  Job& job = jobs_[attempt.slot];
  if (!job.used || job.generation != attempt.generation ||
      std::strncmp(job.sequenceId,
                   attempt.sequenceId,
                   sizeof(job.sequenceId)) != 0) {
    return;
  }

  ++successfulPublishes_;
  job.initialConfirmed = true;
  if (attempt.requestCount > job.confirmedCount) {
    job.confirmedCount = attempt.requestCount;
  }
  job.lastSuccessMs = nowMs;
  job.failureSeriesStartedMs = 0;
  job.failuresInSeries = 0;

  if (job.desiredCount > job.confirmedCount) {
    job.nextAttemptMs = job.closedFinal
                            ? nowMs
                            : nowMs +
                                  AppConfig::kNotificationUpdateIntervalMs;
  } else if (job.closedFinal) {
    removeSlot(attempt.slot);
  } else {
    job.nextAttemptMs = UINT64_MAX;
  }
}

void DeliveryManager::recordFailure(const DeliveryAttempt& attempt,
                                    uint64_t nowMs) {
  if (!attempt.valid || attempt.slot >= kPersistentDeliveryJobCapacity) {
    return;
  }

  Job& job = jobs_[attempt.slot];
  if (!job.used || job.generation != attempt.generation ||
      std::strncmp(job.sequenceId,
                   attempt.sequenceId,
                   sizeof(job.sequenceId)) != 0) {
    return;
  }

  ++failedPublishes_;
  if (job.failureSeriesStartedMs == 0) {
    job.failureSeriesStartedMs = attempt.startedMs;
  }
  if (job.failuresInSeries < UINT8_MAX) {
    ++job.failuresInSeries;
  }
  job.nextAttemptMs = retryDueFor(job, nowMs);
}

void DeliveryManager::restore(const PersistentDeliveryJob* jobs,
                              size_t count,
                              uint64_t nowMs) {
  begin();
  if (jobs == nullptr) {
    return;
  }

  const size_t limit = count < kPersistentDeliveryJobCapacity
                           ? count
                           : kPersistentDeliveryJobCapacity;
  for (size_t i = 0; i < limit; ++i) {
    const PersistentDeliveryJob& persisted = jobs[i];
    if (persisted.used == 0U || persisted.sequenceId[0] == '\0' ||
        persisted.desiredCount == 0U ||
        (persisted.closedFinal != 0U &&
         persisted.initialConfirmed != 0U &&
         persisted.confirmedCount >= persisted.desiredCount)) {
      continue;
    }

    Job& job = jobs_[i];
    job.used = true;
    job.stage = static_cast<NotificationStage>(persisted.stage);
    job.initialConfirmed = persisted.initialConfirmed != 0U;
    job.closedFinal = persisted.closedFinal != 0U;
    job.urgent = persisted.urgent != 0U;
    job.generation = ++generationCounter_;
    job.desiredCount = persisted.desiredCount;
    job.confirmedCount = persisted.confirmedCount;
    job.lastPressEpochValid = persisted.lastPressEpochValid != 0U;
    job.lastPressEpoch = static_cast<time_t>(persisted.lastPressEpoch);
    job.nextAttemptMs =
        (!job.initialConfirmed || job.desiredCount > job.confirmedCount)
            ? nowMs
            : UINT64_MAX;
    std::strncpy(job.sequenceId,
                 persisted.sequenceId,
                 sizeof(job.sequenceId) - 1U);
  }
}

void DeliveryManager::exportPersistent(PersistentDeliveryJob* jobs,
                                       size_t count) const {
  if (jobs == nullptr) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    jobs[i] = PersistentDeliveryJob{};
  }

  const size_t limit = count < kPersistentDeliveryJobCapacity
                           ? count
                           : kPersistentDeliveryJobCapacity;
  for (size_t i = 0; i < limit; ++i) {
    const Job& job = jobs_[i];
    if (!job.used) {
      continue;
    }

    PersistentDeliveryJob& persisted = jobs[i];
    persisted.used = 1U;
    persisted.stage = static_cast<uint8_t>(job.stage);
    persisted.initialConfirmed = job.initialConfirmed ? 1U : 0U;
    persisted.closedFinal = job.closedFinal ? 1U : 0U;
    persisted.lastPressEpochValid = job.lastPressEpochValid ? 1U : 0U;
    persisted.urgent = job.urgent ? 1U : 0U;
    persisted.generation = job.generation;
    persisted.desiredCount = job.desiredCount;
    persisted.confirmedCount = job.confirmedCount;
    persisted.lastPressEpoch = static_cast<int64_t>(job.lastPressEpoch);
    std::strncpy(persisted.sequenceId,
                 job.sequenceId,
                 sizeof(persisted.sequenceId) - 1U);
  }
}

DeliveryStatus DeliveryManager::status(uint64_t nowMs) const {
  DeliveryStatus result{};
  result.successfulPublishes = successfulPublishes_;
  result.failedPublishes = failedPublishes_;
  result.supersededJobs = supersededJobs_;
  result.queueFullEvents = queueFullEvents_;
  for (const auto& job : jobs_) {
    if (!job.used) {
      continue;
    }
    ++result.usedJobs;
    if (job.nextAttemptMs <= nowMs) {
      ++result.dueJobs;
    }
  }
  return result;
}

bool DeliveryManager::hasPendingJobs() const {
  for (const auto& job : jobs_) {
    if (job.used) {
      return true;
    }
  }
  return false;
}

bool DeliveryManager::hasDueJob(uint64_t nowMs) const {
  for (const auto& job : jobs_) {
    if (job.used && job.nextAttemptMs <= nowMs) {
      return true;
    }
  }
  return false;
}


bool DeliveryManager::containsSequence(const char* sequenceId) const {
  return findBySequence(sequenceId) >= 0;
}

DeliveryAggregate DeliveryManager::collapseUnreported() {
  DeliveryAggregate aggregate{};
  for (auto& job : jobs_) {
    if (!job.used || job.desiredCount == 0U) {
      continue;
    }

    const uint32_t unreportedPresses =
        job.desiredCount > job.confirmedCount
            ? job.desiredCount - job.confirmedCount
            : 0U;
    if (unreportedPresses > 0U) {
      aggregate.totalPresses += unreportedPresses;
      ++aggregate.occasionCount;
      if (job.lastPressEpochValid && job.lastPressEpoch > 0 &&
          (!aggregate.latestEpochValid ||
           job.lastPressEpoch > aggregate.latestEpoch)) {
        aggregate.latestEpochValid = true;
        aggregate.latestEpoch = job.lastPressEpoch;
      }
    }
    job = Job{};
  }
  ++generationCounter_;
  return aggregate;
}

int DeliveryManager::findBySequence(const char* sequenceId) const {
  if (sequenceId == nullptr || sequenceId[0] == '\0') {
    return -1;
  }
  for (uint8_t i = 0; i < kPersistentDeliveryJobCapacity; ++i) {
    if (jobs_[i].used &&
        std::strncmp(jobs_[i].sequenceId,
                     sequenceId,
                     sizeof(jobs_[i].sequenceId)) == 0) {
      return i;
    }
  }
  return -1;
}

int DeliveryManager::findFreeSlot() const {
  for (uint8_t i = 0; i < kPersistentDeliveryJobCapacity; ++i) {
    if (!jobs_[i].used) {
      return i;
    }
  }
  return -1;
}

uint64_t DeliveryManager::retryDueFor(const Job& job,
                                      uint64_t nowMs) const {
  uint64_t due = nowMs + AppConfig::kRetryAfterSeriesMs;
  switch (job.failuresInSeries) {
    case 1:
      due = job.failureSeriesStartedMs + AppConfig::kRetryOffset1Ms;
      break;
    case 2:
      due = job.failureSeriesStartedMs + AppConfig::kRetryOffset2Ms;
      break;
    case 3:
      due = job.failureSeriesStartedMs + AppConfig::kRetryOffset3Ms;
      break;
    case 4:
      due = job.failureSeriesStartedMs + AppConfig::kRetryOffset4Ms;
      break;
    case 5:
      due = nowMs + AppConfig::kRetryAfterSeriesMs;
      break;
    default:
      due = nowMs + AppConfig::kRetryOfflineIntervalMs;
      break;
  }

  if (due <= nowMs) {
    due = nowMs + AppConfig::kMinimumRetrySpacingMs;
  }
  return due;
}

uint8_t DeliveryManager::priorityScore(const Job& job) const {
  uint8_t score = 0;
  if (!job.initialConfirmed) {
    score += 100U;
  }
  if (job.urgent) {
    score += 20U;
  }
  if (!job.closedFinal) {
    score += 10U;
  }
  return score;
}

void DeliveryManager::removeSlot(uint8_t slot) {
  if (slot >= kPersistentDeliveryJobCapacity) {
    return;
  }
  jobs_[slot] = Job{};
  ++generationCounter_;
}

void DeliveryManager::refreshDueAfterDesiredChange(Job& job,
                                                   uint64_t nowMs) {
  if (!job.initialConfirmed) {
    if (job.failuresInSeries == 0U && job.nextAttemptMs == UINT64_MAX) {
      job.nextAttemptMs = nowMs;
    }
    return;
  }

  if (job.desiredCount <= job.confirmedCount ||
      job.failuresInSeries > 0U) {
    return;
  }

  const uint64_t normalDue =
      job.lastSuccessMs + AppConfig::kNotificationUpdateIntervalMs;
  job.nextAttemptMs = normalDue <= nowMs ? nowMs : normalDue;
}
