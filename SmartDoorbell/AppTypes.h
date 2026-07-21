#pragma once

#include <Arduino.h>

// Shared log levels for current and future modules.
enum class LogLevel : uint8_t {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

// Minimal application lifecycle used in Stage 1.
// Later stages will extend the project without changing this base interface.
enum class AppLifecycleState : uint8_t {
  Starting,
  Running,
  Fault,
};

struct RuntimeStats {
  uint32_t uptimeMs = 0;
  uint32_t loopCount = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t minimumFreeHeapBytes = 0;
  uint32_t maximumFreeBlockBytes = 0;
  uint8_t heapFragmentationPercent = 0;
};
