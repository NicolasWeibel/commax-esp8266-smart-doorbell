# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses stage-based pre-release versions during development.

## [Unreleased]

### Added

- Stage 2 D5/GPIO14 input configuration with the existing external 10 kΩ pull-up.
- Interrupt-driven `LOW/HIGH` edge capture.
- Fixed-size single-producer/single-consumer raw edge queue.
- Bell detector state machine with:
  - 5-second startup grace period;
  - 2-second stable-LOW arming requirement;
  - 80 ms minimum valid pulse;
  - 15-second maximum valid pulse;
  - abnormal long-HIGH classification;
  - 100 ms stable-LOW rearm requirement;
  - long-HIGH monitoring when `HIGH` begins before rearming completes.
- High-level bell event queue for future session handling.
- Input resynchronization and dropped-event counters.
- Bell detector state and counters in periodic diagnostics.
- 300 ms built-in LED confirmation for valid bell events.
- Microsecond-preserving short-pulse diagnostics.
- Host-side tests for boot arming, valid pulses, sub-millisecond rejection, 100 ms rearm behavior, rapid second presses, long-HIGH handling during rearm, queued edges, and startup while D5 is HIGH.
- Stage 2 hardware, architecture, and validation documentation.

### Changed

- Firmware version updated to `0.2.0-stage2` on the Stage 2 branch.
- Public serial output and firmware comments expanded for bell detection.
- Roadmap consolidated so all connected features after Stage 2 are delivered in one complete Stage 3.

### Validation pending

- Arduino compilation on the target ESP8266 toolchain.
- Direct D5-to-GND simulation test.
- PC817 optocoupler test.
- Real COMMAX intercom test with the handset both on-hook and off-hook.
- Extended hardware stability test.

## [0.1.0-stage1] - 2026-07-20

### Added

- Initial Arduino project for the Wemos D1 mini ESP8266.
- Minimal `SmartDoorbell.ino` entry point.
- Centralized compile-time configuration in `AppConfig.h`.
- Shared base types in `AppTypes.h`.
- `Diagnostics` module with:
  - serial boot report;
  - reset reason;
  - runtime memory metrics;
  - heartbeat every 10 seconds;
  - detailed memory report every 60 seconds;
  - non-blocking built-in LED pulse;
  - configurable heap warnings.
- Public `Secrets.example.h` template for future stages.
- Architecture, hardware safety, testing, roadmap, contribution, and security documentation.

### Validated

- More than 10 minutes of stable hardware operation.
- Two manual reset tests.
- One USB power-cycle test.
- No unexpected resets.
- Stable `52,096 B` free heap and `0%` heap fragmentation during the test.
