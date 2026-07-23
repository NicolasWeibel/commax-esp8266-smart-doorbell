#pragma once

#include <Arduino.h>

enum class LogLevel : uint8_t {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

enum class AppLifecycleState : uint8_t {
  Starting,
  Running,
  Fault,
};

enum class BellDetectorState : uint8_t {
  BootWait,
  WaitingForStableLow,
  Armed,
  HighInProgress,
  HighDuringRearm,
  HighTooLong,
  WaitingForRearm,
};

enum class BellEventType : uint8_t {
  BootWaitFinished,
  Armed,
  PulseStarted,
  ValidPress,
  ShortPulseIgnored,
  HighTooLong,
  RecoveredFromHigh,
  RawQueueOverflow,
  InputResynchronized,
};

// Raw D5 transition captured by the interrupt service routine.
struct EdgeEvent {
  uint32_t timestampUs = 0;
  bool levelHigh = false;
};

// High-level event produced by BellDetector and consumed by the application.
// Pulse duration is retained in microseconds so sub-millisecond electrical
// bounce remains visible in diagnostics instead of being rounded down to 0 ms.
struct BellEvent {
  BellEventType type = BellEventType::BootWaitFinished;
  uint32_t occurredAtMs = 0;
  uint32_t pulseDurationUs = 0;
  uint32_t value = 0;
};

struct BellDetectorStats {
  uint32_t rawEdges = 0;
  uint32_t validPresses = 0;
  uint32_t shortPulsesIgnored = 0;
  uint32_t highTooLongEvents = 0;
  uint32_t rawEdgesDropped = 0;
  uint32_t detectorEventsDropped = 0;
  uint32_t inputResynchronizations = 0;
};

struct BellDetectorStatus {
  BellDetectorState state = BellDetectorState::BootWait;
  bool currentLevelHigh = false;
  uint8_t pendingRawEdges = 0;
  BellDetectorStats stats{};
};

struct RuntimeStats {
  uint32_t uptimeMs = 0;
  uint32_t loopCount = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t minimumFreeHeapBytes = 0;
  uint32_t maximumFreeBlockBytes = 0;
  uint8_t heapFragmentationPercent = 0;
};
