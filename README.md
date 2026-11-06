# Pick-and-Place Robot — REC Task 2, Group 5

Firmware and hardware documentation for a two-wheel pick-and-place robot. An
ESP32 handheld controller reads two HW-504 joysticks and sends commands over
Bluetooth Classic to an HC-05 connected to an Arduino Nano. The Nano controls
an L298N motor driver and a claw servo.

## Repository guide

| File | Purpose |
| --- | --- |
| [`controller_code_ESP32.ino`](controller_code_ESP32.ino) | Main ESP32 controller firmware |
| [`arduino_nano_robot_RX_TX.ino`](arduino_nano_robot_RX_TX.ino) | Main Arduino Nano robot firmware |
| [`joystick_serial_monitor_test.ino`](joystick_serial_monitor_test.ino) | Standalone ESP32 joystick and switch diagnostic |
| [`OVERVIEW.md`](OVERVIEW.md) | Operator controls and project overview |
| [`GUIDE.md`](GUIDE.md) | Detailed build, wiring, flashing, and troubleshooting guide |
| [`hardware/schematic_rev1.1.png`](hardware/schematic_rev1.1.png) | Corrected wiring schematic |
| [`tests/`](tests) | Host-side controller, receiver, and integration regression tests |

## Current behavior

The current firmware includes the following control and reliability changes:

- Both motor channels use centralized polarity flags, so every drive, turn,
  spin, and single-wheel command follows the corrected breadboard wiring.
- Motor PWM is set to `100`; curved turns run the inside wheel at `65`.
- The ESP32 samples the two claw switches in a dedicated task every 5 ms and
  debounces them for 20 ms. Sampling continues while Bluetooth connection
  attempts block the main Arduino task.
- A claw press is remembered and repeated in later packets, so a short click or
  one dropped packet does not lose the requested position.
- The Nano parses newline-terminated packets without blocking and stops both
  motors after 500 ms without a valid packet.
- Movement and claw commands are handled independently; the claw can open or
  close while the robot is stopped or moving.

The Bluetooth connection design has not changed during these control fixes:
the ESP32 remains the Bluetooth Classic master, connects to the HC-05 by name,
and sends the same two-character packet followed by a newline.

## Hardware compatibility

Use an original ESP32 variant that supports Bluetooth Classic and the Arduino
`BluetoothSerial` library. ESP32-C3, ESP32-S2, and ESP32-H2 boards cannot run
this controller sketch because they do not provide the required Classic
Bluetooth SPP stack.

### ESP32 controller wiring

| Signal | ESP32 pin |
| --- | --- |
| Left joystick `VRX` | GPIO32 |
| Left joystick `VRY` | GPIO33 |
| Left joystick `SW` | GPIO25 |
| Right joystick `VRX` | GPIO34 |
| Right joystick `VRY` | GPIO35 |
| Right joystick `SW` | GPIO26 |
| Both joystick `VCC` pins | 3V3 |
| Both joystick `GND` pins | GND |

The switch inputs use `INPUT_PULLUP`: released reads `HIGH`, and pressing the
stick vertically until it clicks connects `SW` to ground and reads `LOW`.
GPIO34 and GPIO35 are not 5 V tolerant, so power the joysticks from 3.3 V.

### Arduino Nano robot wiring

| Device signal | Nano pin | Notes |
| --- | --- | --- |
| L298N `ENA` | D6 | Left motor PWM |
| L298N `IN1` / `IN2` | D5 / D4 | Left motor direction |
| L298N `IN3` / `IN4` | D3 / D2 | Right motor direction |
| L298N `ENB` | D11 | Right motor PWM |
| Claw servo signal | D9 | Servo library uses Timer1 |
| HC-05 `TXD` | D0 / RX | Hardware serial input |
| HC-05 `RXD` | D1 / TX | Hardware serial output; use suitable level shifting |

Remove the L298N `ENA` and `ENB` jumpers so the Nano can control motor speed.
All controller-side and robot-side devices must share ground with their local
power supply.

Use D11 for `ENB`. D7 is not PWM-capable, while D10 shares Timer1 with the
Servo library and cannot provide the intended motor PWM in this design. Power
the servo from a suitable external 5 V supply rather than the Nano regulator,
and connect that supply ground to Nano ground.

Disconnect the HC-05 from D0/D1 while uploading to the Nano because the module
shares the hardware serial port with the USB bootloader.

## Bluetooth setup

The controller expects the HC-05 to advertise the name `module4` and use PIN
`1234`. If your module has a different name or PIN, configure the HC-05 in AT
mode or update `BT_SLAVE_NAME` and the PIN in the ESP32 sketch.

The HC-05 remains in slave mode at 9600 baud. The ESP32 starts in master mode,
attempts the initial connection during setup, and retries every 2 seconds while
the link is unavailable.

## Controls

The X axes control the two drive motors independently. With the other stick
centred:

| Controller action | Robot action | Direction byte |
| --- | --- | --- |
| Left stick forward / backward | Left wheel forward / backward | `W` / `X` |
| Right stick forward / backward | Right wheel forward / backward | `Y` / `Z` |
| Both sticks forward / backward | Drive forward / backward | `F` / `B` |
| Sticks in opposite X directions | Spin in place | `T` / `U` |
| Either Y axis toward left / right | Curve left / right | `L` / `R` |
| Both sticks centred | Stop | `S` |

Pressing the left stick opens the claw; pressing the right stick closes it.
If both switch presses are accepted at the same time, open has priority. The
servo targets are 160° open, 20° closed, and 90° at Nano startup.

## Packet protocol

The ESP32 sends a heartbeat every 100 ms:

```text
<direction><claw>\n
```

For example, `FO\n` means drive forward and open the claw.

| Claw byte | Meaning |
| --- | --- |
| `O` | Move to the open angle |
| `C` | Move to the closed angle |
| `N` | No claw command yet; hold the current position |

The Nano acts only on frames containing at least a direction and claw byte and
terminated by `\r` or `\n`. Unknown direction bytes stop the motors. If no valid
frame arrives for 500 ms, the link watchdog also stops the motors.

## Motor polarity for the current wiring

The final breadboard wiring uses these Nano settings:

```cpp
const bool LEFT_MOTOR_REVERSED  = true;
const bool RIGHT_MOTOR_REVERSED = false;
```

This produces the following direction levels:

| Motor | Forward | Reverse |
| --- | --- | --- |
| Left | `IN1=LOW`, `IN2=HIGH` | `IN1=HIGH`, `IN2=LOW` |
| Right | `IN3=HIGH`, `IN4=LOW` | `IN3=LOW`, `IN4=HIGH` |

If the motor leads or driver connections are changed later, adjust only the
corresponding polarity flag. The shared wheel helpers apply the setting to all
movement commands.

## Build and upload

1. Open `controller_code_ESP32.ino` in Arduino IDE, select the correct
   Bluetooth Classic-capable ESP32 board and port, then upload it.
2. Disconnect the HC-05 from Nano D0/D1.
3. Open `arduino_nano_robot_RX_TX.ino`, select **Arduino Nano**, the correct
   processor/bootloader option, and port, then upload it.
4. Reconnect the HC-05, power the robot, and then power or reset the controller.
5. Open the ESP32 Serial Monitor at 115200 baud to observe connection status,
   joystick readings, accepted claw events, and transmitted packets.

Perform the first movement test with the wheels raised. Verify left-only and
right-only forward/reverse, both wheels together, turns, spins, and the claw.
Finally, interrupt the Bluetooth link while driving and confirm that the motors
stop after the 500 ms watchdog timeout.

## Diagnostics

For joystick wiring checks, upload `joystick_serial_monitor_test.ino` and open
the ESP32 Serial Monitor at 115200 baud. Each joystick should rest near the
middle of its ADC range and its button should change from released to pressed.

The main controller logs accepted button events even when the HC-05 is offline:

```text
CLAW: left=PRESSED right=RELEASED -> OPEN (O)
CLAW: left=RELEASED right=PRESSED -> CLOSE (C)
```

If no event appears, check the joystick `SW` wire, GPIO25/GPIO26 assignment,
and common ground. Firmware cannot detect a switch signal that does not reach
the configured input.

If the ESP32 prints packets ending in `O` or `C` but the servo does not move,
temporarily set `DEBUG_SERIAL` to `1` in the Nano sketch and inspect received
frames at 9600 baud while the robot is stationary. Then check the D9 signal,
external 5 V servo supply, common ground, and mechanical binding. Restore
`DEBUG_SERIAL` to `0` for normal driving because serial logging at 9600 baud can
delay packet processing.

## Regression tests

The host tests compile the actual sketches against lightweight Arduino,
Bluetooth, Servo, and FreeRTOS stubs:

```bash
python3 tests/run_tests.py
```

The suite covers controller mapping and debounce behavior, button sampling
during blocking Bluetooth connection attempts, Nano packet parsing and motor
outputs, claw commands across movement states, link timeout behavior, and the
end-to-end controller-to-receiver packet path.

These tests require a C++ compiler. They validate firmware logic but do not
replace compilation with the real Arduino board packages or physical hardware
testing.
