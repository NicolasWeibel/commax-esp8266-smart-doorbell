# COMMAX ESP8266 Smart Doorbell

ESP8266 firmware that adds text notifications to a classic two-wire COMMAX
intercom through an isolated PC817 input, while retaining the original handset.

This is an independent, unofficial project and is not affiliated with COMMAX.

## Project status

The latest published, hardware-tested release is **Stage 2 (`v0.2.0-stage2`)**.
The `stage-3-complete-firmware` branch contains the connected firmware candidate:
`1.0.0-rc17-production-text-only-natural-time`.

The corrected connected base has been tested over USB and with a real COMMAX.
The candidate with local-time notification text has passed automated tests and
ESP8266 compilation; its final hardware check is pending. See
[Validation](docs/VALIDATION.md) for the distinction and measured resources.

## Features

- Interrupt-driven D5/GPIO14 capture with an external 10 kOhm pull-up.
- Active-HIGH pulses: LOW at rest; a valid bell is confirmed on return to LOW.
- Fixed-capacity queues, pulse filtering, startup stabilization and rearming.
- Verified HTTPS text notifications through ntfy, with reusable TLS connections.
- A 60-second initial notification window, cumulative updates and insistence.
- A new occasion after 120 seconds without a valid bell.
- Wi-Fi/DHCP recovery and one persistent summary for undelivered offline events.
- NTP-based local time and last-bell time/date in Spanish notification messages.
- LittleFS persistence with certificate verification and restart recovery.
- Serial diagnostics and aggregated repeated Wi-Fi outage logs.

Example messages:

```text
Tocaron el timbre a las 16:42.
Tocaron el timbre 3 veces en el último minuto. La última fue a las 16:43.
Mientras estuvo sin conexión, tocaron el timbre 3 veces en una sola ocasión. La última fue ayer a las 22:37.
```

If an event's time cannot be determined reliably, the message says so. This
candidate has no camera/image integration, OTA updater or remote diagnostic
server. Optional static notification links can be configured in `Secrets.h`.

## Hardware and requirements

- Wemos D1 mini or compatible ESP8266 board.
- PC817 module and external 10 kOhm pull-up, wired as in [Hardware](docs/HARDWARE.md).
- Arduino IDE 2.x and ESP8266 Arduino Core **3.1.2**.
- Board: `LOLIN(WEMOS) D1 R2 & mini`; CPU: **160 MHz**.
- Flash layout: **4 MB with 2 MB LittleFS (4M2M)** for the validated installation.
- Serial monitor: **115200 baud**; 2.4 GHz Wi-Fi with DHCP.

## Build and installation

1. Read the wiring and isolation instructions in [Hardware](docs/HARDWARE.md).
2. Copy `SmartDoorbell/Secrets.example.h` to `SmartDoorbell/Secrets.h` and set your
   private Wi-Fi, ntfy and POSIX timezone values. `ART3` is UTC-3 year-round.
3. Open `SmartDoorbell/SmartDoorbell.ino` and select the board settings above.
4. On a new device, generate the certificate store with
   `tools/generate_cert_store.ps1` and provision it in LittleFS before using TLS.
5. Compile and upload the sketch. On an existing installation, preserve its flash
   layout and LittleFS; do not erase all flash or upload the filesystem again
   merely to update the sketch when the certificate store is already valid.
6. Confirm the detector is armed at LOW, time has synchronized and connectivity
   is ONLINE. Follow [Testing](docs/TESTING.md) for physical and software checks.

The currently installed firmware needs one USB upload to receive this candidate.
It cannot be updated remotely with the software presently installed.

## Automated checks

Host tests need Python 3.9+ and a C++17 compiler:

```bash
bash tests/host/run_tests.sh
python tests/host/check_ntfy_idle.py
```

PowerShell users can run `./tests/host/run_tests.ps1`. See
[Host tests](tests/host/README.md) for compiler options and coverage.

Run the publication check on a source tree without private configuration:

```bash
python tests/static/check_release.py
```

The publication check deliberately rejects `Secrets.h`. Automated checks do not
replace compilation for ESP8266 or the final physical test.

## Repository structure

```text
SmartDoorbell/    Arduino sketch and firmware modules
docs/            Current public architecture, hardware, testing and validation
tests/host/      Current logic, message and clock-regression tests
tests/static/    Publication and firmware-invariant checks
tools/           Certificate provisioning and optional PC network probe
.github/         Issue and pull request templates
```

Public documentation is maintained in English. Local setup guides, personal
test reports, older candidate notes and build outputs are excluded from Git.

## Documentation and releases

- [Architecture](docs/ARCHITECTURE.md)
- [Hardware and electrical safety](docs/HARDWARE.md)
- [Testing](docs/TESTING.md)
- [Validation status](docs/VALIDATION.md)
- [Roadmap](docs/ROADMAP.md)
- [Contribution and release workflow](CONTRIBUTING.md)
- [Security policy](SECURITY.md)

Keep `main` on the latest hardware-tested stage. Develop Stage 3 on
`stage-3-complete-firmware` and publish candidates as pre-releases until their
hardware checks are complete. Never publish locally configured binaries,
credentials, device identifiers or unsanitized logs.

Released under the [MIT License](LICENSE).
