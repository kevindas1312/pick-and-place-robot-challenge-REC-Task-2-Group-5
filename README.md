## Documentation

| Document | What it's for |
| --- | --- |
| **`OVERVIEW.md`** | What the project is, what it can and cannot do, and the **operator's manual** — how to hold the controller and drive it. Read this first to understand the product. |
| **`GUIDE.md`** | **Start here if you're touching code.** Plain-English explanation of every line of all three sketches, flashing steps, front-panel gestures and first power-up tests. |
| `README.md` (this file) | Quick reference: pin tables, wire protocol, safety notes. |
| `hardware/schematic_rev1.1.png` | The corrected wiring diagram. Every pin name in the code matches it. |

---

## Repository Contents

This repository contains the source code and documentation for the **Pick-and-Place Robot Challenge (REC - Task 2)** for **Group 5**. Below is an overview of the included modules:

*   **Joystick Testing Module**
    *   Designed for the **ESP32** microcontroller paired with dual **HW-504 joystick modules**.
    *   Verifies and outputs real-time directional toggle readings along both the X and Y axes.

*   **Bluetooth Connectivity Module**
    *   Tests reliable wireless communication and pairing between the **ESP32**, the **HC-05 Bluetooth module**, and the **Arduino Nano**.

*   **Robot Movement Control**
    *   Executes core directional navigation commands (**Forward, Backward, Left, and Right**) for the robot chassis.

---

## Sketches

| File | Target board | Role |
| --- | --- | --- |
| `joystick_serial_monitor_test.ino` | ESP32 | Bench test: prints raw joystick readings, no Bluetooth |
| `controller_code_ESP32.ino` | ESP32 | Reads both joysticks and transmits commands to the robot |
| `arduino_nano_robot_RX_TX.ino` | Arduino Nano | Receives commands, drives the L298N and the claw servo |

## Wiring

### ESP32 controller

| Signal | Pin |
| --- | --- |
| Left stick `VRX` / `VRY` / `SW` | GPIO 32 / 33 / 25 |
| Right stick `VRX` / `VRY` / `SW` | GPIO 34 / 35 / 26 |
| Joystick `VCC` | **3V3** (do not use 5 V: GPIO 34/35 are not 5 V tolerant) |

Stick buttons use the internal pull-ups (`INPUT_PULLUP`), so no external resistors are needed.

### Arduino Nano robot

| Signal | Pin | Note |
| --- | --- | --- |
| `ENA` (left PWM) | D6 | PWM |
| `IN1` / `IN2` (left dir) | D5 / D4 | |
| `IN3` / `IN4` (right dir) | D3 / D2 | |
| `ENB` (right PWM) | D11 | PWM - **must** be a PWM pin (D3, D5, D6, D9, D10, D11) |
| Claw servo signal | D9 | Servo library reserves D9/D10 (Timer1) |
| HC-05 `TXD` / `RXD` | D0 (RX) / D1 (TX) | Hardware serial, **9600** baud |

Remove the `ENA`/`ENB` jumpers on the L298N so the PWM pins actually control the speed, and share a common ground between the Nano, the L298N and the HC-05.

> **ENB pin warning.** Schematic REV 1.0 draws `ENB` on **D10** and an earlier firmware revision used **D7**. Both are wrong:
> * D7 is not a PWM pin at all (only D3, D5, D6, D9, D10, D11 are), so `analogWrite()` degenerates to on/off.
> * D10 *is* PWM, but the Servo library takes over Timer1 for the claw servo, so Timer1 runs in the servo's 20 ms fast-PWM mode. `analogWrite(D10, 200)` then only sets `OCR1B = 200` out of `ICR1 = 39999`, i.e. **0.5 % duty** - the right wheel never turns.
>
> Use **D11** (Timer2A) and leave D9/D10 to the servo.

> When uploading a new sketch, unplug the HC-05 from D0/D1 - it shares the serial port used by the USB bootloader.

## Bluetooth link

1. Power the HC-05 and confirm its name with `AT+NAME?` (factory default `module4`; set it with `AT+NAME=`). That exact string must match `BT_SLAVE_NAME` in `controller_code_ESP32.ino`.
2. Leave the HC-05 in **slave** mode at **9600** baud; the ESP32 runs in master mode and connects to it on boot.
3. If the connection fails, the controller retries every 2 seconds - check the module is powered and in range.

## Control protocol

The ESP32 sends one frame every 100 ms (also used as the robot's heartbeat):

```
<direction><claw>\n          e.g.  "FO\n"  = forward, open claw
```

| Direction byte | Action |
| --- | --- |
| `S` | Stop (also sent when the sticks are centred) |
| `F` / `B` | Forward / backward (both sticks pushed together) |
| `L` / `R` | Steer left / right (Y axis) |
| `T` | Spin in place - left wheel reverse, right wheel forward |
| `U` | Spin in place - left wheel forward, right wheel reverse |
| `W` / `X` | Left wheel forward / reverse only |
| `Y` / `Z` | Right wheel forward / reverse only |
| anything else | Treated as stop |

| Claw byte | Action |
| --- | --- |
| `O` | Open claw (left stick button) |
| `C` | Close claw (right stick button) |
| `N` | No change - hold the current position |

Each stick axis is evaluated on its own against the neutral window (`1000`-`3000` out of `0`-`4095`), so a deflected axis on the other stick never cancels a command.

## Safety

*   **Link failsafe:** if the Nano sees no valid frame for `LINK_TIMEOUT_MS` (500 ms), it stops the motors. A lost Bluetooth link can never leave the robot driving away.
*   **Framing:** only newline-terminated two-byte payloads are executed, and unknown commands default to stop, so noise on the link cannot produce motion.
*   **Tuning knobs** live at the top of each sketch: `motorSpeed` / `turnSpeed` (Nano), `AXIS_LOW` / `AXIS_HIGH` / `SEND_INTERVAL_MS` (ESP32). Set `DEBUG_SERIAL` to `1` in the Nano sketch to echo received frames - keep it `0` while driving.

## Control fixes and verification

The left motor now uses `IN1=HIGH`, `IN2=LOW` for forward, reversing the
previous firmware setting to address the reported backward motion. Motor lead
polarity and stick mounting still need a physical check: raise the wheels,
push the left stick forward, and confirm that the controller prints `W` and
the left wheel moves forward. If it prints `X`, check the joystick's mounting
and raw X readings instead of changing motor wiring.

The right motor now uses `RIGHT_MOTOR_REVERSED = true`: forward sets
`IN3=LOW`, `IN4=HIGH`, and reverse sets the opposite levels. This addresses
the subsequent report of inverted right-wheel motion without changing the
working left motor. Verify that pushing the right stick forward prints `Y`
and pulling it back prints `Z`. If forward instead prints `Z`, the right
stick's axis orientation also needs checking; report its raw X readings.
Bluetooth pairing, reconnect logic, frame format and send interval are unchanged.

Claw buttons are sampled by a dedicated ESP32 task, targeting a 5 ms sampling
interval with a 20 ms debounce, independently of the 100 ms Bluetooth heartbeat.
The task starts before Bluetooth initialization and keeps sampling during
blocking connection attempts. Previously, sampling in `loop()` paused during
those attempts and could miss an entire click. A press sets a remembered target: left
opens, right closes, and releasing holds that target. A new press overrides
a held button; simultaneous presses prefer open. The controller repeats the
target in subsequent packets so one missed packet does not lose the command.
`N` is sent until the first button press. This supersedes the older button
sampling examples in GUIDE.md.

Every accepted press now prints a `CLAW:` event at **115200 baud**, even with
the HC-05 off. For separate left/right clicks, expect:

```text
CLAW: left=PRESSED right=RELEASED -> OPEN (O)
CLAW: left=RELEASED right=PRESSED -> CLOSE (C)
```

These messages mean a button command was accepted locally, not that the robot
received it. Once connected, the last target is included in the regular packets.
The heartbeat's raw button display now says `RELEASED` instead of `OPEN` to
avoid confusing switch state with claw position. A short click can be released
before that display updates; the `CLAW:` event records its detection.

Run host regression checks with `python3 tests/run_tests.py` (requires `g++`,
or set `CXX` to a C++ compiler). These execute both sketches with simulated
Arduino I/O; they do not compile the board libraries or verify the hardware.
The blocking-connection regression simulates background button samples during
startup and reconnection while `loop()` cannot run, checking both event logs
and the next transmitted claw command. It does not emulate FreeRTOS scheduling.
The integration check passes the controller's formatted packet bytes into
the Nano parser and checks the resulting servo angles and right motor outputs.

After uploading **both** sketches, with the wheels raised, check left-only
and right-only forward/reverse, both wheels forward/reverse, then tap left/right buttons
while stopped and driving. The claw should open/close and hold after release.
If packets show `O`/`C` but the servo does not move, check its D9 signal,
external 5 V supply, and common ground. Finally, turn off the controller
while driving and confirm the wheels stop after the 500 ms link timeout.

### Claw stays at its startup position

The current software tests reproduce successful open/close commands; they
do not establish the cause of a physical claw staying at 90 degrees.
Use these checks to locate the failure before changing pins or angles:

1. With both sticks centred, open the ESP32 Serial Monitor at **115200** baud.
   Click the left stick down (its push-button, not its directional axis),
   then release and click the right stick. Expect the `CLAW:` events above,
   and, when connected, packets `SO` then `SC`. Hold each click for half
   a second to also see `Btn: PRESSED` in the heartbeat display.
2. If a button never produces a `CLAW:` event, check its `SW` connection to GPIO25
   (left) or GPIO26 (right) and the joystick's common ground. Simultaneously
   clicking both requests open; it is not an open/close toggle.
3. If the controller sends `SO`/`SC`, temporarily enable `DEBUG_SERIAL` in
   the Nano sketch, upload it, reconnect the HC-05 and inspect the Nano
   monitor at **9600** baud. Check that the received claw byte is `O`/`C`.
   Restore `DEBUG_SERIAL` to `0` after this stationary diagnostic.
4. If the Nano receives the correct bytes but the claw remains at 90 degrees,
   check the servo signal on D9, its external supply under load, common
   ground, and whether the claw mechanism binds. Startup movement alone
   does not verify that the servo can reach 160 or 20 degrees under load.

Report the ESP32 button states/packets and Nano received bytes if the fault
persists. The independent sampler fixes missed clicks during blocking operations;
it cannot detect a switch whose signal never reaches the configured GPIO input.
