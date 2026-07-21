# Hardware and Electrical Safety

## Target system

The final project is designed for a classic two-wire COMMAX intercom connected to a Wemos D1 mini through a PC817 optocoupler module.

The optocoupler provides electrical isolation between:

- the intercom line; and
- the 3.3 V ESP8266 circuit.

## Stage 1 setup

Stage 1 requires only:

- a Wemos D1 mini or compatible ESP8266 board;
- a USB data cable;
- a computer running Arduino IDE.

The firmware does not configure D5 and does not read the intercom or optocoupler yet.

## Planned isolated interface

The planned output-side connection is:

```text
Wemos 3V3 ── 10 kΩ pull-up ──┬── D5
                              └── PC817 output V1

Wemos GND ─────────────────────── PC817 output G
```

The intercom side must remain electrically isolated from Wemos ground when the optocoupler channel jumper is removed.

## Safety rules

- Never connect the intercom line directly to an ESP8266 GPIO.
- Never apply 5 V, 6 V, or the intercom voltage directly to D5 or any 3.3 V GPIO.
- Never short `3V3` directly to `GND`.
- Keep the optocoupler input ground separate from the Wemos ground.
- Disconnect USB power before changing wiring.
- Verify polarity and voltage with a multimeter before connecting the intercom side.
- Do not work on mains wiring. This project only uses low-voltage intercom and USB circuits.
- Confirm the exact module pin labels before applying power; similar PC817 boards may use different layouts.

## Disclaimer

This is a do-it-yourself electronics project. Incorrect wiring may damage the ESP8266, optocoupler module, intercom, computer USB port, or other equipment. Perform all checks carefully and at your own risk.
