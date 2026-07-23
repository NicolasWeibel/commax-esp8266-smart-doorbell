# Roadmap

The project is developed in hardware-tested stages. The `main` branch should contain only the latest approved stage.

## Stage 1 — Firmware foundation and diagnostics

**Status: complete, hardware-tested, and released as `v0.1.0-stage1`**

- minimal Arduino entry point;
- centralized configuration;
- shared base types;
- serial boot report;
- heartbeat and memory diagnostics;
- non-blocking status LED pulse;
- reset, power-cycle, and stability validation.

## Stage 2 — Bell input and pulse detector

**Status: implementation complete; target hardware validation pending**

- configure D5 with the external 10 kΩ pull-up;
- capture optocoupler output edges through an interrupt;
- use a fixed-size ISR-safe raw edge queue;
- preserve edge timestamps when the main loop is delayed;
- wait 5 seconds after boot;
- require 2 seconds of stable `LOW` before arming;
- accept `HIGH` pulses from 80 ms through 15 seconds;
- classify longer `HIGH` periods as abnormal;
- require 100 ms of stable `LOW` before rearming;
- monitor an early `HIGH` during rearm and still classify it if it remains active for more than 15 seconds;
- expose overflow, resynchronization, and pulse counters;
- validate with direct D5 simulation, PC817, and the real COMMAX intercom.

## Stage 3 — Complete connected smart doorbell

**Status: planned after Stage 2 approval**

Stage 3 will combine all remaining functionality in one development branch and one final release. It will still be implemented through focused modules and commits.

### Bell sessions

- first-notification session;
- cumulative bell counter;
- notification counter checks every 10 seconds;
- one-minute first-notification window;
- separate maximum-priority insistence notification;
- session reset after two minutes without a valid bell event.

### Time and persistence

- extended monotonic clock;
- NTP time anchor;
- LittleFS storage;
- atomic offline counter persistence;
- recovery after reset or power interruption;
- exact, reconstructed, lower-bound, or unavailable event time states.

### Wi-Fi, ntfy, and offline mode

- Wi-Fi reconnection;
- text and image notifications;
- notification replacement by stable sequence identifier;
- high and maximum priority channels;
- connectivity state machine;
- offline confirmation after repeated failures over approximately 30 seconds;
- active-session removal when offline is confirmed;
- persistent offline bell count;
- delayed summary after connectivity recovery.

### Imou snapshots

- access-token acquisition, caching, and renewal;
- enhanced snapshot request;
- measured initial availability delay;
- text-first fallback when an image is delayed;
- download to LittleFS without buffering the full JPG in RAM;
- standard snapshot fallback for invalid enhanced URLs;
- camera-offline, temporary API, and quota error handling;
- image attachment reuse for later counter updates.

### Secure integration

- one HTTPS operation at a time;
- network job scheduling around notification deadlines;
- TLS certificate verification;
- continued interrupt-based D5 capture during network operations;
- full session, camera-failure, offline, reset, and recovery tests;
- 48 to 72-hour final stability validation;
- release as `v1.0.0` after complete hardware approval.
