# Hardware and Electrical Safety

## Target system

The project targets a classic two-wire COMMAX intercom connected to a Wemos D1 mini through a PC817 optocoupler module.

The optocoupler provides electrical isolation between:

- the intercom line; and
- the 3.3 V ESP8266 circuit.

## Stage 2 output-side wiring

```text
Wemos 3V3 ── 10 kΩ pull-up ──┬── D5 / GPIO14
                              └── PC817 output V1

Wemos GND ─────────────────────── PC817 output G
```

The firmware uses `INPUT`, not `INPUT_PULLUP`, because the external 10 kΩ resistor already defines the signal.

Expected logic:

```text
PC817 conducting -> D5 LOW  -> idle
PC817 released   -> D5 HIGH -> active bell pulse
```

## COMMAX input-side wiring

For the measured two-wire installation:

```text
COMMAX terminal 1, positive -> PC817 input IN1
COMMAX terminal 2, negative -> PC817 input G beside IN1
```

The measured installation produced approximately:

```text
Idle:          7 to 8 V DC
Bell pressed:  about 1 V DC
```

These measurements are installation-specific. Verify polarity and voltage before connecting another intercom.

## Isolation requirement

The channel isolation jumper must remain removed. Do not connect the COMMAX negative terminal to Wemos ground.

```text
Intercom input G  !=  Wemos output G
```

They belong to opposite sides of the optocoupler.

## Safe direct-input simulation

The detector can be tested without the intercom or batteries when the 10 kΩ pull-up is already installed:

```text
D5 connected to Wemos GND    -> LOW  -> idle
D5 disconnected from GND     -> HIGH -> bell active
D5 connected to GND again    -> LOW  -> bell finished
```

This is safe only because 3V3 reaches D5 through the 10 kΩ resistor. Never connect 3V3 directly to GND.

For this simulation, disconnect the PC817 output V1 from the D5 node so the optocoupler cannot affect the test level.

## Safety rules

- Never connect the intercom line directly to an ESP8266 GPIO.
- Never apply 5 V, 6 V, or the intercom voltage directly to D5 or another 3.3 V GPIO.
- Never short `3V3` directly to `GND`.
- Keep the optocoupler input ground separate from Wemos ground.
- Disconnect USB power before changing wiring.
- Verify polarity and voltage with a multimeter before connecting the intercom side.
- Do not work on mains wiring. This project only uses low-voltage intercom and USB circuits.
- Confirm the exact module pin labels before applying power; similar PC817 boards may use different layouts.
- Stop immediately if the Wemos, resistor, optocoupler, cable, or USB connector becomes warm.

## Disclaimer

This is a do-it-yourself electronics project. Incorrect wiring may damage the ESP8266, optocoupler module, intercom, computer USB port, or other equipment. Perform all checks carefully and at your own risk.
