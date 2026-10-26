# Testing

See [Validation](VALIDATION.md) for what has actually passed and what remains
pending. These instructions describe the current candidate rather than a sequence
of historical RC-specific test plans.

## Host tests

Requirements: Python 3.9+ and a C++17 compiler such as g++.

```bash
bash tests/host/run_tests.sh
python tests/host/check_ntfy_idle.py
```

On Windows with g++ in PATH:

```powershell
./tests/host/run_tests.ps1
python tests/host/check_ntfy_idle.py
```

Both runners execute the logic and message suites and fail on compilation or test
errors. They must not execute an older binary after a failed build. The separate
idle-clock regression extracts the real ntfy timer method and replaces socket
closure; it does not simulate HTTP/TLS. See [Host test coverage](../tests/host/README.md).

## Publication checks

Run on a clean source export before adding private configuration:

```bash
python tests/static/check_release.py
```

This verifies firmware/release invariants, documentation references and the absence
of private configuration. A local working copy containing `SmartDoorbell/Secrets.h`
is intentionally rejected. Check the source package without removing your working
credentials or weakening the publication check.

## ESP8266 build

Use ESP8266 Core 3.1.2, Wemos D1 mini, 160 MHz and the documented 4M2M flash layout.
Supply your ignored `Secrets.h`. Record the compiler result, memory usage and
firmware identity. Host tests do not replace this build.

On an existing installation, update the sketch without erasing LittleFS or its
certificate store. Provision a store on a new device using the certificate helper.
Do not publish a binary built with private credentials.

## Physical checks

Use the wiring and isolation precautions in [Hardware](HARDWARE.md). Start with
the input at its normal LOW rest level and verify arming before generating pulses.

1. Confirm boot, certificate-store readiness, local time and ONLINE connectivity.
2. Generate a real bell press and confirm delivery and correct last-bell local time.
3. Generate several valid presses and check the count and latest time in the update.
4. Check insistence and the end of an occasion after 120 seconds of silence.
5. When practical, interrupt the access point, generate known presses and restore
   it. Confirm one recovery summary with the expected count and distinct occasions.
6. For restart coverage, reset during a session and while an offline count is saved;
   check timing, persistence and the explicit fallback for an unknown event time.
7. Observe normal installed operation, including Wi-Fi reception with the enclosure
   closed, and record any unexpected reset or memory warning.

Serial `sim` and `sim N` exercise session/delivery logic but bypass the D5 detector
and optocoupler. A software simulation is not a physical-input test. Manual input
simulation is described separately in the hardware guide.

## Network diagnostics

At 115200 baud, `help` lists the commands. `status`, `wifihealth`, `wifievents`
and `diag` expose state, link history and DNS/TCP/TLS probes.

For the optional PC-side TCP probe, run `tools/start_local_probe.ps1`, configure
the actual PC address with `diagpc <PC_LAN_ADDRESS> 18080`, and run `diag`.
Disable the target with `diagpc off` and stop the PC helper when finished. This
listener is on the PC; it does not add a remote console to the Wemos.

Publish only a concise, sanitized validation summary in [Validation](VALIDATION.md).
Keep personal logs, addresses, device identifiers and build evidence outside Git.
