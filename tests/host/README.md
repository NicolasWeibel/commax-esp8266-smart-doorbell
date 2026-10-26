# Host tests

Run from the repository root with a C++17 compiler and Python 3.9+:

```bash
bash tests/host/run_tests.sh
python tests/host/check_ntfy_idle.py
```

On Windows:

```powershell
./tests/host/run_tests.ps1
python tests/host/check_ntfy_idle.py
```

The PowerShell runner accepts `-Compiler <path>` and `-CompilerArguments <args>`
for a compatible compiler. Both runners stop on compilation or test failure;
executables are generated in the ignored `tests/host/build/` directory.

## Active suites

| File | Coverage |
| --- | --- |
| `DoorbellLogicTests.cpp` | Sessions, confirmation baselines, offline/recovery policy, restored deadlines, Wi-Fi outage aggregation, detector overflow and queued pulse batches, including microsecond rollover. |
| `MessageFormatterTests.cpp` | Actual message formatter and offline manager: last-bell time, local dates, midnight/year/leap-day boundaries, stored and relative event time, restart uncertainty and invalid-age fallback. |
| `check_ntfy_idle.py` | Extracted production timer method and field: idle deadline before, across and after the 32-bit rollover, exact-zero activity and disconnected closure. |

`Arduino.h` provides only the host operations needed by these suites. The message
suite substitutes `TimeKeeper::formatLocalTime` with host C-library formatting.
The idle test substitutes socket closure instead of emulating the HTTP/TLS stack.

The runners compile the logic and message suites; run the separate idle-clock
command as well. Historical RC-specific copies are not public entry points.

These tests do not establish physical D5/PC817 behavior, certificate provisioning,
real NTP or delivery on a phone. See [Testing](../../docs/TESTING.md) and
[Validation](../../docs/VALIDATION.md) for ESP8266 compilation and hardware checks.
