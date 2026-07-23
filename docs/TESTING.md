# Testing

## Validation environment

- Board: Wemos D1 mini
- Arduino board profile: `LOLIN(WEMOS) D1 R2 & mini`
- ESP8266 Arduino Core: `3.1.2`
- CPU frequency: `80 MHz`
- Serial baud rate: `115200`
- Firmware candidate: `0.2.0-stage2`

## Validation status

Stage 1 has been hardware-tested. Stage 2 is an implementation candidate until every required test below passes on the target board and the real COMMAX installation.

## 1. Compile and upload

1. Open `SmartDoorbell/SmartDoorbell.ino`.
2. Select `LOLIN(WEMOS) D1 R2 & mini`.
3. Select the correct serial port.
4. Verify the sketch.
5. Upload it to the board.

Expected result: compilation and upload complete without external libraries or missing headers.

## 2. Boot with D5 LOW

Start with D5 held at `LOW` through the optocoupler or a safe D5-to-GND test jumper.

Expected sequence:

```text
[INFO] INPUT | initial_D5=LOW
[INFO] DETECTOR | boot_wait_finished | D5=LOW | waiting_for_stable_LOW=2000ms
[INFO] DETECTOR | ARMED | D5=LOW | valid_presses=0
```

The detector should arm approximately seven seconds after initialization: five seconds of startup wait plus two seconds of stable `LOW`.

## 3. Valid bell pulses

After `ARMED` appears:

1. Change D5 from `LOW` to `HIGH`.
2. Keep it `HIGH` for approximately 0.5 to 2 seconds.
3. Return D5 to `LOW`.
4. Wait for rearming before repeating.

Expected result:

```text
[INFO] BELL | pulse_started | LOW->HIGH
[INFO] BELL | VALID | duration=...ms | total=1
[INFO] DETECTOR | ARMED | D5=LOW | valid_presses=1
```

The built-in LED should pulse for approximately 300 ms when the valid event is confirmed.

Repeat at least five times and confirm that the total increases exactly once per valid pulse.

## 4. Stable-LOW rearm

After a valid pulse, generate another `HIGH` transition before D5 has remained `LOW` for 100 ms.

Expected result: the early transition does not create another valid event. While it remains `HIGH`, the heartbeat may show `HIGH_DURING_REARM`. After returning to `LOW`, the detector restarts the 100 ms stable-LOW interval before emitting `ARMED` again.

Then repeat with at least 100 ms of continuous `LOW` between two presses. Both presses must be counted, even if the main loop did not print `ARMED` before the second rising edge.

## 5. Short pulse

Generate a pulse shorter than 80 ms. This is difficult by hand and is best tested with a switch, signal generator, or host-side test.

Expected result:

```text
[INFO] BELL | SHORT_IGNORED | duration_us=... | threshold_us=80000 | ignored_total=1
```

The valid bell count must not increase.

## 6. HIGH longer than 15 seconds

1. Wait for `ARMED`.
2. Change D5 to `HIGH`.
3. Keep it `HIGH` for more than 15 seconds.
4. Return it to `LOW`.

Expected result:

```text
[WARN] SIGNAL | HIGH_TOO_LONG | duration=...ms | limit=15000ms | total=1
[INFO] SIGNAL | returned_to_LOW | high_duration=...ms | waiting_for_rearm
```

The valid bell count must not increase.

Repeat the long-HIGH test starting less than 100 ms after a previous pulse returned to `LOW`. The detector must still emit `HIGH_TOO_LONG`, proving that the rearm state cannot hide a disconnected or unavailable input.

## 7. Boot while D5 is HIGH

1. Hold D5 at `HIGH`.
2. Reset the Wemos.
3. Wait beyond the five-second startup period.

Expected result:

```text
[INFO] INPUT | initial_D5=HIGH
[INFO] DETECTOR | boot_wait_finished | D5=HIGH | waiting_for_stable_LOW=2000ms
```

It must not arm or create a false bell event while D5 remains `HIGH`.

Return D5 to `LOW` and keep it there for two seconds. The detector should then arm with the valid bell count unchanged.

## 8. Direct D5-to-GND simulation

Use this test before connecting the PC817 or COMMAX:

1. Confirm the 10 kΩ resistor is between 3V3 and D5.
2. Disconnect the PC817 output V1 from D5.
3. Connect D5 to Wemos GND to simulate idle.
4. Start the firmware and wait for `ARMED`.
5. Remove only the D5 end of the jumper to simulate the bell.
6. Reconnect it after one second to complete the pulse.

Never connect 3V3 directly to GND.

## 9. PC817 optocoupler test

After the direct D5 test passes:

1. Restore the PC817 output V1 connection.
2. Apply a safe, module-compatible low-voltage signal to the isolated PC817 input.
3. Confirm that input power produces D5 `LOW`.
4. Interrupt the input signal and confirm D5 becomes `HIGH`.
5. Restore the input signal and confirm one valid event.

The Wemos ground and optocoupler input ground must remain isolated.

## 10. Real COMMAX test

After the direct-input and PC817 tests pass:

1. Connect COMMAX terminal 1 to `IN1`.
2. Connect COMMAX terminal 2 to the input-side `G` beside `IN1`.
3. Keep the isolation jumper removed.
4. Power the Wemos by USB.
5. Confirm idle produces D5 `LOW` and the detector arms.
6. Press the outdoor bell three times with the handset on-hook.
7. Press it three times with the handset off-hook.
8. Confirm the original intercom still rings and operates normally.

Expected result: exactly one valid event for each real press, with no dropped edges, dropped events, or resynchronizations.

## 11. Periodic detector report

Every 60 seconds, confirm a line similar to:

```text
[INFO] DETECTOR_STATUS | state=ARMED | D5=LOW | pending_edges=0 | raw_edges=... | valid=... | short=... | long_high=... | dropped_edges=0 | dropped_events=0 | resyncs=0
```

For normal tests, these values should remain zero:

```text
dropped_edges=0
dropped_events=0
resyncs=0
```

## 12. Stability test

Leave the complete Stage 2 setup running for at least 30 minutes and generate occasional bell events.

Expected result:

- no watchdog resets;
- no exceptions;
- no unexpected restarts;
- no progressive heap loss;
- no increasing heap fragmentation;
- no dropped edges or high-level events;
- detector normally returns to `ARMED` with D5 `LOW`.

## Host tests

The `tests/host` directory contains a lightweight fake-Arduino environment that runs the actual `BellDetector.cpp` logic on a desktop compiler. These tests supplement but do not replace target-board and electrical validation.

## Stage 2 approval criteria

Stage 2 may be merged into `main` and tagged `v0.2.0-stage2` only after:

- target-toolchain compilation passes;
- direct D5 simulation passes;
- PC817 test passes;
- real COMMAX tests pass on-hook and off-hook;
- the 30-minute stability test passes;
- all privacy-sensitive logs are sanitized before publication.
