# Roadmap

The project is developed in hardware-tested stages. The `main` branch should contain only the latest approved stage.

## Stage 1 — Firmware foundation and diagnostics

**Status: complete and hardware-tested**

- minimal Arduino entry point;
- centralized configuration;
- shared base types;
- serial boot report;
- heartbeat and memory diagnostics;
- non-blocking status LED pulse;
- reset, power-cycle, and stability validation.

## Stage 2 — Bell input and pulse detector

- configure D5 with an external 10 kΩ pull-up;
- capture optocoupler output edges;
- use a fixed-size interrupt-safe event queue;
- wait 5 seconds after boot;
- require 2 seconds of stable LOW before arming;
- accept HIGH pulses from 80 ms to 15 seconds;
- classify longer HIGH periods as abnormal;
- require 500 ms of stable LOW before rearming.

## Stage 3 — Bell sessions

- first notification session;
- cumulative bell counter;
- counter update check every 10 seconds;
- one-minute first-notification window;
- separate maximum-priority insistence notification;
- session reset after two minutes without a valid bell event.

## Stage 4 — Time and persistent offline state

- extended monotonic clock;
- NTP time anchor;
- LittleFS storage;
- atomic offline counter persistence;
- recovery after reset or power interruption;
- exact, reconstructed, lower-bound, or unavailable event time states.

## Stage 5 — ntfy text notifications and connectivity

- text notifications;
- notification replacement by stable sequence identifier;
- high and maximum priority channels;
- connectivity state machine;
- offline confirmation after repeated failures;
- delayed offline summary after connectivity recovery.

## Stage 6 — Imou snapshots

- access-token acquisition and renewal;
- enhanced snapshot request;
- measured snapshot availability delay;
- file download to LittleFS;
- text-first fallback when an image is delayed;
- standard snapshot fallback for invalid enhanced URLs;
- camera-offline and API error handling.

## Stage 7 — Full integration and secure TLS

- network job scheduler;
- one HTTPS operation at a time;
- secure certificate validation;
- notification image attachment reuse;
- end-to-end session and offline tests;
- long-duration stability and memory testing.
