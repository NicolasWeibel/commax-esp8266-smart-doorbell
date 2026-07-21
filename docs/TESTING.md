# Testing

## Validation environment

- Board: Wemos D1 mini
- Arduino board profile: `LOLIN(WEMOS) D1 R2 & mini`
- ESP8266 Arduino Core: `3.1.2`
- CPU frequency: `80 MHz`
- Serial baud rate: `115200`
- Firmware version: `0.1.0-stage1`

## Stage 1 test procedure

### 1. Compile and upload

1. Open `SmartDoorbell/SmartDoorbell.ino`.
2. Select the Wemos D1 mini board profile.
3. Select the correct serial port.
4. Verify the sketch.
5. Upload it to the board.

Expected result: compilation and upload complete without external libraries or missing headers.

### 2. Boot report

Open the serial monitor at `115200` baud and reset the board.

Confirm that the report includes:

- firmware name and version;
- implementation stage;
- ESP8266 Arduino Core version;
- SDK version;
- reset reason;
- CPU frequency;
- physical flash size;
- sketch size and free sketch space;
- current heap metrics.

A short unreadable line may appear before the report because the ESP8266 bootloader uses a different baud rate.

### 3. Heartbeat

Wait at least 35 seconds.

Expected result: heartbeat messages appear approximately every 10 seconds, and the built-in LED pulses briefly at the same interval.

### 4. Detailed memory report

Wait at least 60 seconds.

Expected result: a detailed memory report appears with free heap, largest free block, fragmentation, and minimum free heap since boot.

### 5. Reset and power-cycle tests

- Press the physical reset button twice.
- Disconnect USB power, wait several seconds, and reconnect it.

Expected result: the firmware restarts normally and resumes heartbeat output. No restart should occur without an intentional reset or power interruption.

### 6. Stability test

Leave the board running for at least 10 minutes.

Expected result:

- no watchdog resets;
- no exceptions;
- no unexplained restarts;
- no progressive heap loss;
- no increasing heap fragmentation;
- a consistent loop counter rate.

## Verified Stage 1 results

The hardware test completed successfully:

- stable operation beyond 10 minutes;
- two manual reset tests passed;
- one USB power-cycle test passed;
- no unexpected resets;
- free heap remained at `52,096 B` throughout the test;
- largest free block remained at `52,096 B`;
- heap fragmentation remained at `0%`;
- heartbeat and detailed memory intervals behaved as configured.

## Scope limitation

Stage 1 does not test D5, the PC817 optocoupler, the COMMAX line, Wi-Fi, ntfy, Imou, or LittleFS. Those tests belong to later stages.
