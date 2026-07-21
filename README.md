# COMMAX ESP8266 Smart Doorbell

An open-source ESP8266 firmware project that adds smart notifications and camera snapshots to a classic two-wire COMMAX intercom without replacing the original indoor unit.

> This is an independent, unofficial project. It is not affiliated with or endorsed by COMMAX.

## Project status

This repository currently contains **Stage 1 (`0.1.0-stage1`)**, a stable and observable firmware foundation for the Wemos D1 mini. Bell detection, networking, notifications, image capture, and persistent offline reporting are intentionally deferred to later stages.

Stage 1 provides:

- a minimal Arduino entry point;
- centralized compile-time configuration;
- a reusable diagnostics module;
- a serial boot report;
- a heartbeat every 10 seconds;
- a detailed memory report every 60 seconds;
- non-blocking status LED pulses;
- runtime heap and fragmentation monitoring;
- no external library dependencies beyond the ESP8266 Arduino Core.

## Target hardware

- Wemos D1 mini or compatible ESP8266 board
- COMMAX two-wire intercom
- PC817 optocoupler module
- 10 kΩ pull-up resistor

Only the Wemos D1 mini and a USB data cable are required to validate Stage 1. The intercom input is not read yet.

## Requirements

- Arduino IDE 2.x
- ESP8266 Arduino Core 3.1.2
- Board: `LOLIN(WEMOS) D1 R2 & mini`
- Serial monitor: `115200` baud

## Quick start

1. Open `SmartDoorbell/SmartDoorbell.ino` in Arduino IDE.
2. Select `LOLIN(WEMOS) D1 R2 & mini`.
3. Select the correct serial port.
4. Verify and upload the sketch.
5. Open the serial monitor at `115200` baud.
6. Reset the board and confirm the boot report.
7. Observe at least three heartbeat messages.

The expected output includes:

```text
Firmware: COMMAX ESP8266 Smart Doorbell 0.1.0-stage1
Implementation: Stage 1 - base structure and diagnostics
[INFO] HEARTBEAT | uptime=10s | heap=... | minimum=... | fragmentation=... | loops=...
[INFO] MEMORY | free=... | largest_block=... | fragmentation=... | minimum_since_boot=...
```

## Repository structure

```text
commax-esp8266-smart-doorbell/
├── SmartDoorbell/          Arduino sketch and firmware modules
├── docs/                   Public architecture, hardware, testing, and roadmap
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

Never commit `SmartDoorbell/Secrets.h`, Wi-Fi credentials, Imou credentials, private ntfy topics, device identifiers, signed snapshot URLs, or unsanitized logs. Use `Secrets.example.h` only as a template.

See [SECURITY.md](SECURITY.md) for the complete policy.

## Versioning

The project uses stage-based pre-release versions while the architecture is being developed:

```text
v0.1.0-stage1
v0.2.0-stage2
v0.3.0-stage3
```

The `main` branch should always contain the latest hardware-tested stage.

## License

Released under the [MIT License](LICENSE).
