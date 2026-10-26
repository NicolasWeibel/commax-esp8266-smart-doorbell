#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"

class DeliveryManager {
 public:
  void begin();

  bool ensureSequence(NotificationStage stage,
                      const char* sequenceId,
                      uint32_t count,
                      bool lastPressEpochValid,
                      time_t lastPressEpoch,
                      bool urgent,
                      uint64_t nowMs,
                      uint32_t confirmedBaseline = 0U);
  bool updateSequence(const char* sequenceId,
                      uint32_t count,
                      bool lastPressEpochValid,
                      time_t lastPressEpoch,
                      uint64_t nowMs);
  bool forceSequence(const char* sequenceId,
                     uint32_t count,
                     bool lastPressEpochValid,
                     time_t lastPressEpoch,
                     uint64_t nowMs);
  bool closeSequence(const char* sequenceId,
                     uint32_t count,
                     bool lastPressEpochValid,
                     time_t lastPressEpoch,
                     uint64_t nowMs);
  bool supersedeSequence(const char* sequenceId,
                         uint32_t* confirmedCount = nullptr);

  bool nextDue(uint64_t nowMs, DeliveryAttempt& attempt) const;
  void recordSuccess(const DeliveryAttempt& attempt, uint64_t nowMs);
  void recordFailure(const DeliveryAttempt& attempt, uint64_t nowMs);

  void restore(const PersistentDeliveryJob* jobs, size_t count, uint64_t nowMs);
  void exportPersistent(PersistentDeliveryJob* jobs, size_t count) const;

  DeliveryStatus status(uint64_t nowMs) const;
  bool hasPendingJobs() const;
  bool hasDueJob(uint64_t nowMs) const;
  bool containsSequence(const char* sequenceId) const;
  DeliveryAggregate collapseUnreported();

 private:
  struct Job {
    bool used = false;
    NotificationStage stage = NotificationStage::None;
    bool initialConfirmed = false;
    bool closedFinal = false;
    bool urgent = false;
    uint32_t generation = 0;
    uint32_t desiredCount = 0;
    uint32_t confirmedCount = 0;
    bool lastPressEpochValid = false;
    time_t lastPressEpoch = 0;
    uint64_t lastSuccessMs = 0;
    uint64_t nextAttemptMs = 0;
    uint64_t failureSeriesStartedMs = 0;
    uint8_t failuresInSeries = 0;
    char sequenceId[64]{};
  };

  int findBySequence(const char* sequenceId) const;
  int findFreeSlot() const;
  uint64_t retryDueFor(const Job& job, uint64_t nowMs) const;
  uint8_t priorityScore(const Job& job) const;
  void removeSlot(uint8_t slot);
  void refreshDueAfterDesiredChange(Job& job, uint64_t nowMs);

  Job jobs_[kPersistentDeliveryJobCapacity]{};
  uint32_t generationCounter_ = 0;
  uint32_t successfulPublishes_ = 0;
  uint32_t failedPublishes_ = 0;
  uint32_t supersededJobs_ = 0;
  uint32_t queueFullEvents_ = 0;
};
