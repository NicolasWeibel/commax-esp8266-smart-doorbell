# Architecture

## Stage 1 objective

Stage 1 establishes a minimal, modular, and observable firmware foundation before adding electrical input handling or network services. Its purpose is to validate the build environment, board startup, serial diagnostics, memory behavior, and non-blocking loop structure.

## Design principles

- Keep the Arduino entry point small.
- Centralize configuration values.
- Separate diagnostics from application logic.
- Avoid dynamic memory allocation in the base loop.
- Avoid long blocking delays after startup.
- Yield to the ESP8266 core on every loop iteration.
- Add hardware and network responsibilities incrementally and test each stage independently.

## Firmware modules

### `SmartDoorbell.ino`

Arduino entry point. It initializes the diagnostics service in `setup()` and calls its `tick()` method in `loop()`.

### `AppConfig.h`

Central compile-time configuration:

- firmware name and version;
- serial baud rate;
- heartbeat and memory-report intervals;
- built-in LED behavior;
- informational memory thresholds;
- compile-time log level.

### `AppTypes.h`

Shared types used by current and future modules:

- `LogLevel`;
- `AppLifecycleState`;
- `RuntimeStats`.

### `Diagnostics.h` and `Diagnostics.cpp`

Responsibilities:

- initialize the serial console;
- print the boot report;
- collect runtime memory statistics;
- emit periodic heartbeat messages;
- emit detailed memory reports;
- pulse the built-in LED without blocking the main loop;
- report low-memory and fragmentation warnings.

### `Secrets.example.h`

A public template for future private configuration. Stage 1 does not include or use `Secrets.h`.

## Runtime flow

```text
setup()
└── Diagnostics::begin()
    ├── configure the built-in LED
    ├── initialize Serial
    ├── sample memory state
    ├── print the boot report
    └── enter the Running lifecycle state

loop()
├── Diagnostics::tick()
│   ├── update runtime counters
│   ├── sample heap metrics
│   ├── update the non-blocking LED pulse
│   ├── print heartbeat when due
│   └── print detailed memory report when due
└── yield()
```

## Deferred responsibilities

The following components are intentionally not implemented in Stage 1:

- D5 input and optocoupler edge capture;
- bell-pulse validation;
- session and insistence state machines;
- Wi-Fi and NTP;
- LittleFS persistence;
- ntfy notifications;
- Imou authentication and snapshots;
- TLS certificate management;
- offline event reporting.

Each responsibility will be introduced as an isolated module in a later stage.
