# Architecture

## Stage 2 objective

Stage 2 adds reliable electrical bell detection without introducing networking, filesystem access, or dynamic allocation in the timing-sensitive path. The detector must continue to work correctly when later stages add HTTPS operations that may temporarily delay the main loop.

## Signal model

The PC817 output is connected to D5/GPIO14 with an external 10 kΩ pull-up to 3V3.

```text
Idle intercom line  -> optocoupler conducts -> D5 LOW
Bell line active    -> optocoupler releases -> D5 HIGH
```

A valid bell event is confirmed only when the signal returns to `LOW`, because that transition provides the complete pulse duration. Two presses are treated as distinct only when at least 100 ms of continuous `LOW` separates them.

## Design principles

- Keep the Arduino entry point small.
- Capture D5 transitions in a minimal ISR.
- Perform no serial, network, filesystem, JSON, or dynamic-memory work in the ISR.
- Preserve edge timestamps even if the main loop is temporarily delayed.
- Use fixed-capacity queues and explicit overflow counters.
- Use unsigned elapsed-time comparisons to tolerate timer rollover.
- Require a known, stable idle state before arming.
- Never classify a long disconnected or unavailable signal as a bell event.
- Keep detection independent from future Wi-Fi, notification, and camera failures.

## Firmware modules

### `SmartDoorbell.ino`

Arduino entry point. It initializes diagnostics and bell detection, drains high-level bell events, and gives detector processing priority in each loop iteration.

### `AppConfig.h`

Central compile-time configuration:

- firmware identity;
- serial and diagnostic intervals;
- D5 pin and signal polarity;
- startup, pulse, and rearm timing;
- fixed queue capacities;
- LED behavior;
- memory-warning thresholds.

### `AppTypes.h`

Shared types:

- logging and application lifecycle;
- detector states;
- raw edge events;
- high-level bell events;
- detector statistics and status;
- runtime memory statistics.

### `EventQueue.h`

A fixed-size single-producer/single-consumer ring buffer:

- producer: D5 ISR;
- consumer: main loop;
- no heap allocation;
- one slot intentionally reserved to distinguish full from empty;
- dropped-edge counter when the queue is full.

### `BellDetector.h` and `BellDetector.cpp`

Responsibilities:

- configure D5 as an externally pulled-up input;
- attach a `CHANGE` interrupt;
- capture level and `micros()` timestamp in the ISR;
- preserve queued edge occurrence times;
- classify the electrical pulse;
- emit high-level events;
- resynchronize against the physical D5 level if an edge is lost;
- expose status and counters to diagnostics.

### `Diagnostics.h` and `Diagnostics.cpp`

Stage 1 diagnostics plus:

- initial D5 level;
- detector state in each heartbeat;
- bell event logs;
- raw, valid, short, long, dropped, and resynchronization counters;
- microsecond duration output for rejected short pulses;
- a longer LED pulse for valid bell events.

## Detector state machine

```text
BOOT_WAIT
  wait 5 seconds
  |
  v
WAITING_FOR_STABLE_LOW
  require LOW for 2 seconds
  |
  v
ARMED
  LOW -> HIGH
  |
  v
HIGH_IN_PROGRESS
  |-- HIGH -> LOW before 80 ms ------> SHORT_IGNORED
  |-- HIGH -> LOW at 80 ms..15 s ----> VALID_PRESS
  |-- HIGH remains >15 s ------------> HIGH_TOO_LONG
  |
  v
WAITING_FOR_REARM
  require LOW for 100 ms
  |-- HIGH before 100 ms ------------> HIGH_DURING_REARM
  |                                      |-- return LOW: restart 100 ms rearm
  |                                      |-- remain HIGH >15 s: HIGH_TOO_LONG
  |
  +-----------------------------------> ARMED
```

If the board starts while D5 is `HIGH`, it follows the input but never creates a bell event. It arms only after D5 later remains `LOW` for two full seconds.

A `HIGH` transition that starts before the 100 ms rearm interval completes is not counted as a new bell press. It enters `HIGH_DURING_REARM`, where it is ignored as a press but still timed. This prevents contact bounce from becoming a duplicate bell while ensuring a disconnected input or other abnormal `HIGH` lasting more than 15 seconds is still reported.

## ISR path

```text
D5 changes
└── ISR
    ├── read D5
    ├── read micros()
    └── push raw edge into fixed queue
```

The ISR deliberately does not:

- print to Serial;
- allocate memory;
- access LittleFS;
- parse JSON;
- connect to Wi-Fi;
- call Imou or ntfy.

## Main-loop path

```text
loop()
├── BellDetector::tick()
│   ├── consume dropped-edge count
│   ├── drain raw edge queue
│   ├── reconstruct edge occurrence time
│   ├── update state machine
│   ├── verify physical D5 level
│   └── process timed transitions
├── drain high-level bell events
├── Diagnostics::tick()
└── yield()
```

## Deferred responsibilities

Stage 2 intentionally excludes:

- bell sessions and cumulative notification counts;
- Wi-Fi and NTP;
- LittleFS persistence;
- ntfy notifications;
- offline detection and delayed reports;
- Imou authentication and snapshots;
- image downloads and attachments;
- TLS certificate management.

These responsibilities are planned together for Stage 3 after Stage 2 has been validated on the real COMMAX installation.
