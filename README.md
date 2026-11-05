## Documentation

| Document | What it's for |
| --- | --- |
| **`GUIDE.md`** | **Start here.** Plain-English explanation of every line of all three sketches, plus step-by-step instructions for flashing the boards and the first power-up test. No prior Arduino experience assumed. |
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

1. Power the HC-05 and confirm its name with `AT+NAME?` (factory default `HC-05`; set it with `AT+NAME=`). That exact string must match `BT_SLAVE_NAME` in `controller_code_ESP32.ino`.
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
