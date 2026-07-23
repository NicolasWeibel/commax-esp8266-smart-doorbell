# COMMAX ESP8266 Smart Doorbell

An open-source ESP8266 firmware project that adds smart notifications and camera snapshots to a classic two-wire COMMAX intercom without replacing the original indoor unit.

> This is an independent, unofficial project. It is not affiliated with or endorsed by COMMAX.

## Project status

The latest hardware-tested release is **Stage 1 (`v0.1.0-stage1`)**. This branch contains the **Stage 2 implementation candidate (`0.2.0-stage2`)**, which adds isolated bell input detection and must be validated on the target hardware before it is merged into `main` and tagged.

Stage 2 provides:

- D5/GPIO14 input using the external 10 kΩ pull-up;
- active-HIGH bell signal handling (`LOW` idle, `HIGH` active);
- interrupt-driven edge capture;
- a fixed-size ISR-safe raw edge queue;
- a non-blocking bell detector state machine;
- a 5-second startup grace period;
- arming only after 2 seconds of stable `LOW`;
- rejection of pulses shorter than 80 ms;
- valid bell pulses from 80 ms through 15 seconds;
- classification of `HIGH` periods longer than 15 seconds as abnormal;
- rearming after 100 ms of stable `LOW`;
- detection of abnormal long `HIGH` periods even when they begin before rearming completes;
- microsecond diagnostics for rejected short pulses;
- input resynchronization and queue-overflow diagnostics;
- a 300 ms built-in LED pulse after each valid bell event;
- host-side state-machine tests for the core detection scenarios.

Wi-Fi, ntfy, Imou, LittleFS, and persistent offline reporting remain disabled in this stage.

## Target hardware

- Wemos D1 mini or compatible ESP8266 board
- Classic two-wire COMMAX intercom
- PC817 optocoupler module
- 10 kΩ external pull-up resistor

The target optocoupler output wiring is:

```text
Wemos 3V3 ── 10 kΩ ──┬── D5 / GPIO14
                       └── PC817 output V1

Wemos GND ─────────────── PC817 output G
```

The optocoupler input side and Wemos ground must remain isolated when the channel isolation jumper is removed.

## Requirements

- Arduino IDE 2.x
- ESP8266 Arduino Core 3.1.2
- Board profile: `LOLIN(WEMOS) D1 R2 & mini`
- Serial monitor: `115200` baud

No external Arduino libraries are required for Stage 2.

## Quick start

1. Open `SmartDoorbell/SmartDoorbell.ino` in Arduino IDE.
2. Select `LOLIN(WEMOS) D1 R2 & mini`.
3. Select the correct serial port.
4. Verify and upload the sketch.
5. Open the serial monitor at `115200` baud.
6. Start with D5 held at `LOW` through the optocoupler or a safe test jumper.
7. Reset the board and wait for `DETECTOR | ARMED`.
8. Generate a `LOW -> HIGH -> LOW` pulse lasting between 80 ms and 15 seconds.

Expected output includes:

```text
Firmware: COMMAX ESP8266 Smart Doorbell 0.2.0-stage2
[INFO] INPUT | initial_D5=LOW
[INFO] DETECTOR | boot_wait_finished | D5=LOW | waiting_for_stable_LOW=2000ms
[INFO] DETECTOR | ARMED | D5=LOW | valid_presses=0
[INFO] BELL | pulse_started | LOW->HIGH
[INFO] BELL | VALID | duration=...ms | total=1
```

Read [Testing](docs/TESTING.md) before connecting the firmware to a real intercom.

## Repository structure

```text
commax-esp8266-smart-doorbell/
├── SmartDoorbell/          Arduino sketch and firmware modules
├── docs/                   Public architecture, hardware, testing, and roadmap
├── tests/host/             Host-side bell detector tests
├── .github/                Issue and pull request templates
├── README.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── LICENSE
├── .gitignore
├── .gitattributes
└── .editorconfig
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Hardware and electrical safety](docs/HARDWARE.md)
- [Testing](docs/TESTING.md)
- [Roadmap](docs/ROADMAP.md)
- [Security policy](SECURITY.md)

## Security and privacy

Never commit `SmartDoorbell/Secrets.h`, Wi-Fi credentials, Imou credentials, private ntfy topics, device identifiers, signed snapshot URLs, real entrance images, or unsanitized logs. `Secrets.example.h` contains placeholders only.

See [SECURITY.md](SECURITY.md) for the complete policy.

## Versioning

The project uses stage-based pre-release versions while the architecture is being developed:

```text
v0.1.0-stage1  Stable firmware foundation
v0.2.0-stage2  Bell input detection
v1.0.0         Complete connected smart doorbell
```

The `main` branch should always contain the latest hardware-tested stage. Stage 2 development belongs on `stage-2-bell-detection` until validation is complete.

## License

Released under the [MIT License](LICENSE).
