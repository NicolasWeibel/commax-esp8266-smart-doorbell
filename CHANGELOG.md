# Changelog

Published stages are retained below. Intermediate, unpublished development
iterations are summarized in the current candidate rather than listed as releases.

## 1.0.0-rc17-production-text-only-natural-time

**Stage 3 candidate; not yet published or hardware-validated in this exact build.**

### Added

- Wi-Fi/DHCP, NTP local time and verified HTTPS ntfy text notifications.
- Grouped notification updates, insistence, scheduled retries and reusable TLS.
- Persistent offline summaries excluding events already confirmed as delivered.
- LittleFS state v4 with migration from v3 and session restoration after restart.
- Last-bell local time/date in notifications, with explicit unknown-time fallback.
- Serial network diagnostics and aggregated repeated Wi-Fi outage reports.
- Current host regression suites, publication checks and certificate-store tools.

### Fixed

- Prevent diagnostic events from crowding valid bell events out of the queue.
- Preserve remaining session deadlines when restoring events from an earlier boot.
- Keep ntfy idle timing correct across the 32-bit `millis()` rollover.
- Reject a failed host compilation instead of executing a stale test binary.
- Preserve recovery completion safeguards for input active during publication.

### Validation

- Host logic/message tests, idle-clock regressions and source publication checks pass.
- Complete sketch compiles with ESP8266 Core 3.1.2, 160 MHz and the 4M2M layout.
- The corrected connected base was tested via USB and with a real COMMAX, including
  offline recovery. The local-time candidate still needs its physical check.
- Detailed scope and resource figures are in [Validation](docs/VALIDATION.md).

OTA updates, remote diagnostic services and camera snapshots are outside this
candidate's scope. Public documentation and tests describe the current candidate;
personal reports and superseded candidate files are kept outside the tracked tree.

## 0.2.0-stage2

**Published hardware-tested bell input stage.**

### Added

- Interrupt-driven D5/GPIO14 input capture through the isolated PC817 output.
- Fixed-size raw-edge and detector-event queues with explicit drop diagnostics.
- Startup stabilization, stable-LOW arming and 100 ms rearming.
- Pulse validation from 80 ms through 15 seconds, short-pulse rejection and
  abnormal long-HIGH detection, including HIGH that starts during rearming.
- Input resynchronization, counters, LED confirmation and host detector tests.

### Validated

- Direct D5 simulation, PC817 and real COMMAX operation.
- Consecutive pulses, contact bounce, prolonged HIGH and unchanged handset operation.

## 0.1.0-stage1

**Published hardware-tested firmware foundation.**

### Added

- Minimal Arduino entry point, centralized configuration and shared types.
- Boot, reset, memory and heartbeat diagnostics with a non-blocking status LED.
- Public documentation, configuration template, security policy and repository metadata.

### Validated

- Initial stability observation, manual reset and USB power-cycle checks.
