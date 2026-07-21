# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses stage-based pre-release versions during development.

## [Unreleased]

### Planned

- D5 input and bell-pulse detection for Stage 2.

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

