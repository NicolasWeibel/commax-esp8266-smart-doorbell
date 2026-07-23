# BellDetector host tests

These tests compile the actual `SmartDoorbell/BellDetector.cpp` implementation against a small fake-Arduino environment.

Covered scenarios:

- startup wait and stable-LOW arming;
- valid one-second pulse;
- exact preservation of a sub-millisecond rejected pulse;
- 100 ms stable-LOW rearm behavior;
- a rapid second press accepted after the 100 ms boundary;
- an early HIGH ignored as a duplicate press;
- long-HIGH classification after 15 seconds, including a HIGH that begins during rearm;
- queued long-HIGH classification during rearm;
- startup while D5 is HIGH without a false bell event;
- pulse timing preserved while edges wait in the ISR queue;
- pulse measurement across the 32-bit `micros()` rollover.

## Linux or macOS

```sh
./tests/host/run_tests.sh
```

## Windows PowerShell

A `g++` compiler must be available in `PATH`:

```powershell
.\tests\host\run_tests.ps1
```

These tests do not validate ESP8266 interrupt timing, board pin behavior, the PC817 module, or the COMMAX line. Hardware validation remains mandatory.
