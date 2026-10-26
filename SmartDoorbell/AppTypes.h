#pragma once

#include <Arduino.h>
#include <time.h>

// -----------------------------------------------------------------------------
// Generic application types
// -----------------------------------------------------------------------------
enum class LogLevel : uint8_t {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

enum class AppLifecycleState : uint8_t {
  Starting,
  Running,
  Degraded,
  Fault,
};

// -----------------------------------------------------------------------------
// Bell detector types
// -----------------------------------------------------------------------------
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

struct EdgeEvent {
  uint32_t timestampUs = 0;
  bool levelHigh = false;
};

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

// -----------------------------------------------------------------------------
// Doorbell session types
// -----------------------------------------------------------------------------
enum class NotificationStage : uint8_t {
  None,
  FirstMinute,
  Insistence,
  RecoverySummary,
};

enum class SessionPhase : uint8_t {
  Idle,
  FirstMinute,
  FirstFinalPending,
  Insistence,
};

enum class BellSessionResult : uint8_t {
  StartedFirst,
  UpdatedFirst,
  StartedInsistence,
  UpdatedInsistence,
};

enum class SessionEventType : uint8_t {
  FirstWindowClosed,
  SessionExpired,
};

struct SessionEvent {
  SessionEventType type = SessionEventType::FirstWindowClosed;
  NotificationStage stage = NotificationStage::None;
  uint32_t totalPresses = 0;
  bool lastPressEpochValid = false;
  time_t lastPressEpoch = 0;
  char sequenceId[64]{};
};

struct SessionStatus {
  bool active = false;
  SessionPhase phase = SessionPhase::Idle;
  uint32_t totalPresses = 0;
  uint64_t firstPressMs = 0;
  uint64_t lastPressMs = 0;
  uint64_t firstWindowEndsMs = 0;
  uint64_t sessionEndsMs = 0;
  bool firstPressEpochValid = false;
  bool lastPressEpochValid = false;
  time_t firstPressEpoch = 0;
  time_t lastPressEpoch = 0;
  char firstSequenceId[64]{};
  char insistenceSequenceId[64]{};
};

// -----------------------------------------------------------------------------
// Delivery and connectivity types
// -----------------------------------------------------------------------------
enum class ConnectivityState : uint8_t {
  WaitingForTime,
  Online,
  Degraded,
  Offline,
  Recovering,
};

enum class OfflineReason : uint8_t {
  None,
  WifiLost,
  PublishFailures,
  FailureTimeout,
  TimeUnavailable,
  RestoredState,
};

struct ConnectivityStatus {
  ConnectivityState state = ConnectivityState::WaitingForTime;
  OfflineReason offlineReason = OfflineReason::None;
  uint8_t consecutiveFailures = 0;
  bool wifiConnected = false;
  uint64_t wifiDisconnectedSinceMs = 0;
  uint64_t firstFailureMs = 0;
  uint64_t offlineSinceMs = 0;
  bool recovered = false;
  bool becameUnavailable = false;
};

enum class NtfyResultCode : uint8_t {
  Success,
  WifiUnavailable,
  TimeUnavailable,
  TlsUnavailable,
  InvalidEndpoint,
  DnsFailed,
  ConnectFailed,
  TlsHandshakeFailed,
  WriteFailed,
  ResponseTimeout,
  InvalidResponse,
  HttpError,
};

struct NtfyPublishResult {
  bool success = false;
  NtfyResultCode code = NtfyResultCode::InvalidResponse;
  int httpStatus = 0;
  int httpClientError = 0;
  int sslError = 0;
  uint32_t responseBytes = 0;
  uint32_t totalMs = 0;
  uint32_t transportGeneration = 0;
  uint16_t localPort = 0;
  uint16_t tcpTimeWaitBefore = 0;
  uint16_t tcpTimeWaitAfter = 0;
  bool connectionReused = false;
  bool reconnectAttempted = false;
  bool reconnectSucceeded = false;
  bool keepAliveRetained = false;
  bool wifiRecycleRequested = false;
  String error{};
};

struct DeliveryAttempt {
  bool valid = false;
  uint8_t slot = 0;
  uint32_t generation = 0;
  NotificationStage stage = NotificationStage::None;
  uint32_t requestCount = 0;
  bool lastPressEpochValid = false;
  time_t lastPressEpoch = 0;
  bool firstSuccessfulPublish = false;
  bool urgent = false;
  uint64_t startedMs = 0;
  char sequenceId[64]{};
};

struct DeliveryStatus {
  uint8_t usedJobs = 0;
  uint8_t dueJobs = 0;
  uint32_t successfulPublishes = 0;
  uint32_t failedPublishes = 0;
  uint32_t supersededJobs = 0;
  uint32_t queueFullEvents = 0;
};

struct DeliveryAggregate {
  uint32_t totalPresses = 0;
  uint32_t occasionCount = 0;
  bool latestEpochValid = false;
  time_t latestEpoch = 0;
};

// -----------------------------------------------------------------------------
// Offline summary types
// -----------------------------------------------------------------------------
enum class LastBellTimeQuality : uint8_t {
  None,
  ExactEpoch,
  RelativeUptime,
  UnknownAfterRestart,
};

struct OfflineSummaryStatus {
  bool active = false;
  bool initialConfirmed = false;
  uint32_t totalPresses = 0;
  uint32_t occasionCount = 0;
  LastBellTimeQuality lastBellTimeQuality = LastBellTimeQuality::None;
  time_t lastBellEpoch = 0;
  uint64_t lastBellMonotonicMs = 0;
  uint32_t lastBellBootNonce = 0;
  bool currentOccasionOpen = false;
  uint64_t lastOccasionBellMs = 0;
  char sequenceId[64]{};
};

// -----------------------------------------------------------------------------
// Persistent state. Fixed-size records avoid heap allocations and allow CRC.
// -----------------------------------------------------------------------------
constexpr uint8_t kPersistentDeliveryJobCapacity = 8;

#pragma pack(push, 1)
struct PersistentSessionState {
  uint8_t active = 0;
  uint8_t phase = static_cast<uint8_t>(SessionPhase::Idle);
  uint8_t firstPressEpochValid = 0;
  uint8_t lastPressEpochValid = 0;
  uint32_t totalPresses = 0;
  int64_t firstPressEpoch = 0;
  int64_t lastPressEpoch = 0;
  char firstSequenceId[64]{};
  char insistenceSequenceId[64]{};
};

struct PersistentDeliveryJob {
  uint8_t used = 0;
  uint8_t stage = static_cast<uint8_t>(NotificationStage::None);
  uint8_t initialConfirmed = 0;
  uint8_t closedFinal = 0;
  uint8_t lastPressEpochValid = 0;
  uint8_t urgent = 0;
  uint8_t reserved0 = 0;
  uint8_t reserved1 = 0;
  uint32_t generation = 0;
  uint32_t desiredCount = 0;
  uint32_t confirmedCount = 0;
  int64_t lastPressEpoch = 0;
  char sequenceId[64]{};
};

struct PersistentOfflineSummary {
  uint8_t active = 0;
  uint8_t lastBellTimeQuality =
      static_cast<uint8_t>(LastBellTimeQuality::None);
  uint8_t initialConfirmed = 0;
  uint8_t reserved1 = 0;
  uint32_t totalPresses = 0;
  uint32_t occasionCount = 0;
  int64_t lastBellEpoch = 0;
  uint64_t lastBellMonotonicMs = 0;
  uint32_t lastBellBootNonce = 0;
  char sequenceId[64]{};
};

// Legacy RC8.1-RC13 layout, used only to migrate existing LittleFS state.
struct PersistentStateRecordV3 {
  uint16_t version = 3;
  uint16_t recordSize = 0;
  uint32_t generation = 0;
  PersistentSessionState session{};
  PersistentDeliveryJob jobs[kPersistentDeliveryJobCapacity]{};
  uint32_t crc32 = 0;
};

struct PersistentStateRecord {
  uint16_t version = 4;
  uint16_t recordSize = 0;
  uint32_t generation = 0;
  PersistentSessionState session{};
  PersistentDeliveryJob jobs[kPersistentDeliveryJobCapacity]{};
  PersistentOfflineSummary offline{};
  uint32_t crc32 = 0;
};
#pragma pack(pop)

static_assert(sizeof(PersistentStateRecordV3) == 900U,
              "Legacy persistent v3 layout changed.");
static_assert(sizeof(PersistentOfflineSummary) == 96U,
              "Offline summary persistent layout changed.");
static_assert(sizeof(PersistentStateRecord) == 996U,
              "Persistent v4 layout changed unexpectedly.");
static_assert(sizeof(PersistentStateRecord) < 2048U,
              "Persistent state unexpectedly exceeds 2 KiB.");

struct RuntimeStats {
  uint32_t uptimeMs = 0;
  uint32_t loopCount = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t minimumFreeHeapBytes = 0;
  uint32_t maximumFreeBlockBytes = 0;
  uint8_t heapFragmentationPercent = 0;
};
