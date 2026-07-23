#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"

// Fixed-size single-producer/single-consumer ring buffer.
// The ISR is the only producer and the main loop is the only consumer.
// The producer publishes the head index only after the event is fully written.
class EdgeEventQueue {
 public:
  EdgeEventQueue() = default;

  bool IRAM_ATTR pushFromIsr(uint32_t timestampUs, bool levelHigh) {
    const uint8_t head = head_;
    const uint8_t next = static_cast<uint8_t>(
        (head + 1U) & (AppConfig::kRawEdgeQueueCapacity - 1U));

    if (next == tail_) {
      ++droppedCount_;
      return false;
    }

    timestampsUs_[head] = timestampUs;
    levels_[head] = levelHigh ? 1U : 0U;
    head_ = next;
    return true;
  }

  bool pop(EdgeEvent& event) {
    noInterrupts();

    const uint8_t tail = tail_;
    if (tail == head_) {
      interrupts();
      return false;
    }

    event.timestampUs = timestampsUs_[tail];
    event.levelHigh = levels_[tail] != 0U;
    tail_ = static_cast<uint8_t>(
        (tail + 1U) & (AppConfig::kRawEdgeQueueCapacity - 1U));

    interrupts();
    return true;
  }

  uint32_t takeDroppedCount() {
    noInterrupts();
    const uint32_t dropped = droppedCount_;
    droppedCount_ = 0;
    interrupts();
    return dropped;
  }

  uint8_t size() const {
    noInterrupts();
    const uint8_t head = head_;
    const uint8_t tail = tail_;
    interrupts();

    return static_cast<uint8_t>(
        (head - tail) & (AppConfig::kRawEdgeQueueCapacity - 1U));
  }

 private:
  volatile uint32_t timestampsUs_[AppConfig::kRawEdgeQueueCapacity]{};
  volatile uint8_t levels_[AppConfig::kRawEdgeQueueCapacity]{};
  volatile uint8_t head_ = 0;
  volatile uint8_t tail_ = 0;
  volatile uint32_t droppedCount_ = 0;
};
