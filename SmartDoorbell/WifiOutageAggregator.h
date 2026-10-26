#pragma once

#include <stdint.h>

struct WifiOutageAggregate {
  uint32_t durationMs = 0;
  uint32_t totalNoApAttempts = 0;
};

// Collapses repeated NO_AP_FOUND callbacks into one first event, bounded
// periodic summaries, and one final recovery summary. This class is kept
// independent from Arduino so the policy can be exercised by host tests.
class WifiOutageAggregator {
 public:
  // Returns true only for the first NO_AP_FOUND in the current outage.
  bool recordNoAp(uint32_t nowMs) {
    if (!active_) {
      active_ = true;
      firstMs_ = nowMs;
      lastSummaryMs_ = nowMs;
      totalAttempts_ = 1;
      return true;
    }

    if (totalAttempts_ < UINT32_MAX) {
      ++totalAttempts_;
    }
    return false;
  }

  bool active() const { return active_; }

  bool periodicDue(uint32_t nowMs, uint32_t intervalMs) const {
    return active_ &&
           static_cast<uint32_t>(nowMs - lastSummaryMs_) >= intervalMs;
  }

  WifiOutageAggregate periodicSummary(uint32_t nowMs) {
    WifiOutageAggregate summary = snapshot(nowMs);
    lastSummaryMs_ = nowMs;
    return summary;
  }

  WifiOutageAggregate finish(uint32_t nowMs) {
    WifiOutageAggregate summary = snapshot(nowMs);
    reset();
    return summary;
  }

  uint32_t totalAttempts() const { return totalAttempts_; }

 private:
  WifiOutageAggregate snapshot(uint32_t nowMs) const {
    WifiOutageAggregate summary{};
    if (!active_) {
      return summary;
    }
    summary.durationMs = static_cast<uint32_t>(nowMs - firstMs_);
    summary.totalNoApAttempts = totalAttempts_;
    return summary;
  }

  void reset() {
    active_ = false;
    firstMs_ = 0;
    lastSummaryMs_ = 0;
    totalAttempts_ = 0;
  }

  bool active_ = false;
  uint32_t firstMs_ = 0;
  uint32_t lastSummaryMs_ = 0;
  uint32_t totalAttempts_ = 0;
};
