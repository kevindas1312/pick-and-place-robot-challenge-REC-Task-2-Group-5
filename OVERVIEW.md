# Project Overview & Operator's Manual

**Pick & Place Robot — REC Task 2 — Group 5**

This is the "what is it and how do I drive it" document. It explains the product as a whole: what
the challenge asks for, what the robot is, what it can and cannot do, and how a person operates it.

If you want to understand *how the code works*, read **`GUIDE.md`**.
If you want the pin tables and wire protocol, read **`README.md`**.
If you want to build or rewire it, open **`hardware/schematic_rev1.1.png`**.

---

## Contents

1. [What this product is](#1-what-this-product-is)
2. [The challenge](#2-the-challenge)
3. [At a glance](#3-at-a-glance)
4. [How the system fits together](#4-how-the-system-fits-together)
5. [What it can do](#5-what-it-can-do)
6. [What it cannot do](#6-what-it-cannot-do)
7. [The controls](#7-the-controls)
8. [How to drive it — operator's manual](#8-how-to-drive-it--operators-manual)
9. [Running the challenge](#9-running-the-challenge)
10. [Parts list](#10-parts-list)
11. [Status and next steps](#11-status-and-next-steps)
12. [Document map](#12-document-map)

---

## 1. What this product is

A **two-part, manually driven pick-and-place robot**.

* A **handheld controller** you hold in both hands. It has two thumb joysticks and two push-buttons
  built into the stick tops. It contains an ESP32 board and a battery.
* A **robot** that drives on two wheels and has a servo-operated claw on the front. It contains an
  Arduino Nano, a motor driver board and a Bluetooth module.

There is **no cable between them** and **no camera**. You drive it by sight, like a remote-control
car, and you use the claw to pick something up and put it down.

It is deliberately **not autonomous**. Nothing in the robot decides where to go — every movement
comes from a human thumb. That is the point of the exercise: the challenge is about building a
reliable *remote control system*, not about writing a self-driving algorithm.

---

## 2. The challenge

The task, as implemented here, is a **teleoperated pick-and-place run**:

1. Start the robot at a known position.
2. Drive it to an object on the field.
3. Close the claw on the object.
4. Drive it — still holding the object — to a target location.
5. Open the claw to release it.
6. Optionally reverse away and repeat.

The robot therefore has to be good at four separate things:

| Requirement | What it demands of the design |
| --- | --- |
| **Reach the object accurately** | predictably straight driving and reliable, repeatable turns |
| **Grab it** | a claw that opens far enough, closes firmly, and holds while driving |
| **Carry it without dropping it** | a grip that does not slip when the chassis bumps |
| **Release it where intended** | fine positioning — being able to nudge one wheel at a time |

> The official task sheet, field dimensions, object specification and scoring rubric are supplied
> separately by the organisers and are **not** part of this repository. Everything in this document
> describes *this* implementation of the task, not the marking scheme.

---

## 3. At a glance

| | |
| --- | --- |
| **Type** | Two-part teleoperated robot, no autonomy |
| **Controller** | ESP32 (WROOM-32 devkit) + 2 × HW-504 analog joysticks |
| **Robot brain** | Arduino Nano (ATmega328P), 16 MHz |
| **Link** | Bluetooth Classic, SPP profile. ESP32 = master, HC-05 = slave |
| **Command rate** | 10 messages per second (one every 100 ms) |
| **Message** | Two ASCII letters + newline, e.g. `FO\n` |
| **Drive** | 2 × TT DC gearmotors through an L298N dual H-bridge |
| **Steering** | Differential (skid steer) — no steering linkage |
| **End effector** | 1 × MG90S micro servo operating a claw |
| **Robot power** | 2S LiPo 7.4 V → L298N directly, and → buck converter → 5 V logic |
| **Controller power** | Its own USB supply (power bank) |
| **Radio range** | HC-05 Class 2 — typically about 10 m in open air. **Measure yours**, and treat walls and bodies as obstacles |
| **Reaction delay** | Roughly 100–150 ms from thumb to wheel (sampling + radio + processing) |
| **Failsafe** | If the robot hears nothing for 500 ms it stops the motors |
| **Operating time** | Depends entirely on the battery pack. Time a full run and leave a margin |

---

## 4. How the system fits together

Nothing in this project is magic. A thumb movement passes through eight stages, and every stage can
be tested on its own — which is exactly why the troubleshooting table later on is organised by
stage.

```
   YOUR THUMB
      │
      ▼
   1  joystick potentiometer ......... outputs a voltage between 0 and 3.3 V
      │
      ▼
   2  ESP32 analogRead() ............. converts that to a number 0 … 4095
      │
      ▼
   3  thresholds + if/else ladder ..... decides ONE of eleven letters
      │
      ▼
   4  10 Hz timer .................... sends every 100 ms
      │
      ▼
   5  Bluetooth radio (2.4 GHz) ...... ESP32 (master) → HC-05 (slave)
      │
      ▼
   6  serial wire at 9600 baud ....... HC-05 → Nano RX pin, one byte at a time
      │
      ▼
   7  newline parser + switch ........ rebuilds "F" + "O", calls the motor functions
      │
      ▼
   8  L298N H-bridge ................. switches motor current: direction + speed
      │
      ▼
   WHEELS TURN
```

| Stage | Lives in | Typical failure | How to test it alone |
| --- | --- | --- | --- |
| 1–2 Sensing | `joystick_serial_monitor_test.ino` | 5 V instead of 3.3 V to the sticks; loose signal wire | run the test sketch, watch the numbers |
| 3 Decision | `controller_code_ESP32.ino` (the ladder) | dead zone wrong; wrong letter for the gesture | Serial Monitor shows `Dir: …` and the packet |
| 4–5 Radio | both, plus the HC-05 module | not paired, out of range, wrong module name | `Connected to HC-05 Successfully!` in the monitor |
| 6–7 Receiving | `arduino_nano_robot_RX_TX.ino` (`serviceLink`) | TX/RX swapped; baud mismatch; divider on the wrong side | set `DEBUG_SERIAL` to `1` and watch the Nano's port |
| 8 Actuation | the motor functions + wiring | `ENB` on the wrong pin; jumpers left on the L298N | the wheels-off-the-ground test |

Two numbers define the robot's "feel", and they come from stages 4 and 7:

* **100 ms** — how often the controller speaks. Longer feels laggy, shorter floods the radio.
* **500 ms** — how long the robot waits before deciding the link is dead. Five missed messages.

---

## 5. What it can do

**Movement**

* Drive forward and backward in a straight line.
* Curve left and right while moving (one wheel slowed down).
* Spin in place, both directions.
* Move a single wheel at a time — essential for nudging into alignment with an object.
* Stop instantly on command, and stop by itself if the radio link dies.

**Manipulation**

* Open and close a claw driven by a servo.
* Hold the current grip indefinitely while driving — the servo keeps its position without being
  re-commanded.

**Practical**

* Fully wireless; no cable between operator and robot.
* Reconnects automatically if the robot is switched on late, goes out of range, or has its battery
  swapped — the controller redials every two seconds.
* Two independent power systems, so a flat controller battery does not affect the robot.
* Every received command is validated before it is acted on: unknown or corrupted bytes cause a
  stop, never a random movement.

---

## 6. What it cannot do

Being honest about this list is more useful than pretending otherwise. If the task needs any of
these, the design has to change.

| Limitation | Consequence | What it would take |
| --- | --- | --- |
| **No driving while steering** | you cannot make a smooth curve while holding a straight line; forward and steering are separate gestures that cannot combine | rewrite the decision ladder to blend both axes |
| **Not proportional** | the sticks are treated as switches. Speed is fixed at `motorSpeed`, so you cannot creep slowly or accelerate | map the stick's deflection to `analogWrite` values instead of thresholds |
| **No position feedback** | no encoders, no odometry. Drive forward for two seconds and the robot has no idea where it is | add wheel encoders and closed-loop control |
| **No obstacle sensing** | it will drive into anything you point it at | add ultrasonic/IR sensors |
| **No grip feedback** | the claw squeezes with exactly as much force as the servo produces, whether or not an object is there | add a force sensor or measure servo current |
| **Fixed claw stroke** | it fits objects in the size range the linkage allows, and nothing else | mechanically adjustable jaws |
| **Radio needs reasonable conditions** | bodies, walls and 2.4 GHz traffic shorten the range. Out of range = the robot stops (by design) | a stronger module, or a lower-frequency link |
| **No battery monitoring** | neither end reports its voltage. The robot simply dies when the pack is flat | a voltage divider into an analog pin, or a LiPo alarm |
| **No low-voltage protection** | a 2S LiPo below ~6.6 V is permanently damaged | a LiPo alarm or a cutoff circuit |
| **One-way trust** | the robot never confirms it received a command; if a message is lost, the previous command simply continues for another 100 ms | add acknowledgements |


---

## 7. The controls

The controller has exactly **six** things you can do: four stick directions and two buttons.

```
        +--------------------------------------------------+
        |                                                  |
        |      +-------+                +-------+          |
        |      |   ^   |                |   ^   |          |
        |      | < o > |                | < o > |          |
        |      |   v   |                |   v   |          |
        |      +-------+                +-------+          |
        |      [CLICK]                  [CLICK]            |
        |                                                  |
        |     LEFT STICK (J1)          RIGHT STICK (J2)    |
        |     IO32 / IO33 / IO25       IO34 / IO35 / IO26  |
        +--------------------------------------------------+
```

Each stick does three jobs:

| Control | Pushed how | Its job |
| --- | --- | --- |
| **Left stick, up/down** | `^` / `v` | left wheel forward / backward |
| **Left stick, left/right** | `<` / `>` | steering (left / right) |
| **Left stick, click** | press the top of the stick | **open** the claw |
| **Right stick, up/down** | `^` / `v` | right wheel forward / backward |
| **Right stick, left/right** | `<` / `>` | steering (left / right) |
| **Right stick, click** | press the top of the stick | **close** the claw |

Two things follow from that table and both surprise people on their first drive:

1. **"Up/down" is not the same axis as "left/right".** Forward and backward live on the vertical
   axis. Steering lives on the horizontal axis. They are read in that order, so pushing a stick
   forward means the horizontal axis is never even looked at.
2. **Either stick can steer.** That is deliberate — it lets you drive with one hand and work the
   claw with the other.

### The dead zone

The sticks have a deliberately **large** dead zone: you must push about a quarter of the way
before anything happens at all. This is not a fault. It stops hand tremor and electrical noise from
making the robot twitch while you are setting up a grab.

When you let go, the stick springs back to centre, the letter becomes `S`, and the robot stops
within about a tenth of a second.

> For a picture-by-picture breakdown of every single gesture and exactly what the robot does, see
> **section 5, Part 9 of `GUIDE.md`** — "The gesture-to-motion cheat sheet".

---

## 8. How to drive it — operator's manual

### 8.1 Pre-flight checks (30 seconds, every time)

- [ ] Robot battery charged, and **above 7 V** measured (below that, expect sluggish motors)
- [ ] Controller battery charged
- [ ] `ENB` still on **D11** (it is easy to knock a jumper wire loose)
- [ ] Both HC-05 wires seated in **D0/D1** on the Nano
- [ ] Claw opens and closes by hand without binding
- [ ] Nothing metallic loose on the field where the robot will drive

### 8.2 Power-up order

**Robot first, controller second.** The controller is the Bluetooth master — it goes looking for
the robot. If it starts first it simply redials every two seconds until the robot appears, which
works but wastes time.

| Step | Do | Look for |
| --- | --- | --- |
| 1 | Switch the robot on | claw twitches to centre, motors stay still, HC-05 LED blinks **fast** |
| 2 | Switch the controller on | within ~5 s the HC-05 LED changes to a **slow double-blink** |
| 3 | Push both sticks forward briefly | wheels spin, robot creeps forward |

If the LED never changes to the double-blink, stop and fix the link — do not start the run.

### 8.3 Driving technique

**The first thing to internalise: forward and steering are separate gestures.**
Push both sticks forward to go straight. To change direction, stop, then push either stick left or
right. Trying to do both at once does nothing useful — the forward command wins and the steering is
ignored.

**Approach the object slowly and in stages.** Stop, check alignment, correct with a single-wheel
nudge, stop again. The single-wheel commands (`W`/`X`/`Y`/`Z`) exist precisely for this: they pivot
the chassis around one wheel so you can fine-tune without overshooting.

**Grab, then lift, then drive.** Close the claw, wait about a second for the servo to reach its
angle, and only then drive. If you drive during the grab, the object can squirt out sideways.

**Stop before you think you need to.** The robot has no brakes, only coasting, so allow a little
run-on.

**Keep the controller pointed at the robot.** Bodies block 2.4 GHz. If the robot stops on its own
mid-run, that is the failsafe detecting a lost link, not a motor fault — move closer and it will
resume within a second.

### 8.4 Shutdown

1. Let go of the sticks (the robot stops).
2. Switch the **controller** off.
3. Switch the **robot** off.
4. If it will be stored for more than a few days, unplug the LiPo — it slowly self-discharges and
   a deeply discharged 2S pack is scrap.

### 8.5 Emergency stop

There are three, in order of preference:

| Method | What happens |
| --- | --- |
| **Release the sticks** | the letter becomes `S`, the motors are commanded off within ~100 ms |
| **Switch the robot off** | power is cut to the motors immediately |
| **Walk away / get behind something** | the link drops, and after 500 ms the robot's watchdog stops the motors by itself |

The third one is the reason the watchdog exists. It is the only one that works when you cannot
reach the robot.


---

## 9. Running the challenge

A repeatable run sheet. Practise it exactly like this so the real attempt has no surprises.

| # | Phase | What the operator does | What success looks like |
| --- | --- | --- | --- |
| 1 | **Set-up** | Place the robot on its start mark, wheels straight, claw open | robot stationary, nothing touching it |
| 2 | **Pre-flight** | the section 8.1 checklist | all boxes ticked |
| 3 | **Link** | robot on, then controller on | HC-05 double-blinks; test-forward works |
| 4 | **Sighting** | stand where you can see the whole field | no obstacle between you and the robot's path |
| 5 | **Approach** | both sticks forward to drive; stop; steer; repeat until the object is between the jaws | object centred in the claw, robot stopped |
| 6 | **Grab** | press the **right** stick's button to close | jaws stop moving on the object, not in mid-air |
| 7 | **Settle** | pause ~1 s | object held, claw not straining or buzzing loudly |
| 8 | **Transport** | drive to the target the same way you came | object still held after a few bumps |
| 9 | **Align** | single-wheel nudges to line up with the target | object directly over the target |
| 10 | **Release** | press the **left** stick's button to open | object settles on the target, claw clear |
| 11 | **Retreat** | both sticks back for a second | robot clear of the placed object |
| 12 | **Shutdown** | sticks released, controller off, robot off | motors silent |

### Timing you can expect

| Action | Delay |
| --- | --- |
| Push a stick → wheels react | ~100–150 ms |
| Release a stick → wheels stop being powered | ~100 ms |
| Link lost → robot stops by itself | 500 ms |
| Controller switched on → connected to a live robot | 1–5 s |
| Controller switched on → gives up on a dead robot | never — it retries forever |
| Claw open → claw closed | ~1 s of servo travel |

### If it goes wrong mid-run

| What you see | First thing to try |
| --- | --- |
| Robot stops dead and won't respond | move closer — that is the failsafe; it resumes when the link returns |
| Robot will only turn one way | `ENB` has come loose; check D11 |
| Robot drifts while the sticks are centred | a stick is not returning to centre, or the dead zone was changed |
| One wheel much weaker than the other | battery getting low, or the `ENB` pin again |
| Claw won't grip | object too wide for the jaws, or the servo is under-powered from a sagging 5 V rail |


---

## 10. Parts list

Matches `hardware/schematic_rev1.1.png` exactly.

### Controller

| Qty | Part | Role | Notes |
| --- | --- | --- | --- |
| 1 | ESP32-WROOM-32 devkit (30-pin) | Brains of the remote | needs a board with **Classic** Bluetooth — not a C3/S2 |
| 2 | HW-504 analog joystick module | Input | wired to **3V3**, never 5 V |
| 1 | USB power bank | Controller power | any 5 V source works |

### Robot electronics

| Qty | Part | Role | Notes |
| --- | --- | --- | --- |
| 1 | Arduino Nano (ATmega328P) | Brains of the robot | clones usually need "Old Bootloader" when flashing |
| 1 | L298N dual H-bridge module | Motor driver | **ENA/ENB jumpers removed**, 5 V jumper fitted |
| 1 | HC-05 Bluetooth module | Radio receiver | must be a **slave** at **9600** baud |
| 1 | DC-DC buck converter | 7.4 V → 5 V | should supply at least 1 A for the servo |
| 1 | 2S LiPo 7.4 V pack | Robot power | plus a matching charger |
| 1 | Rocker switch | Master on/off | on the battery side |
| 1 | **Fuse 3 A + holder** | Short-circuit protection | added in REV 1.1 |
| 1 | `C3` 100 µF | 5 V rail smoothing | |
| 1 | **`C4` 470 µF** | Motor rail smoothing | added in REV 1.1 |
| 2 | **`C1`, `C2` 0.1 µF** | Motor noise snubbers | REV 1.0 wrongly said 0.1 **F** |
| 2 | `R1` 1 kΩ, `R2` 2 kΩ | Level divider for the HC-05 RX pin | on the **Nano TX** side only |
| 1 | `C5` 470 µF | Servo smoothing | *recommended* — not shown as fitted on the drawing |

### Mechanical

| Qty | Part | Role |
| --- | --- | --- |
| 2 | TT DC gearmotor + wheel | Drive, skid steer |
| 1 | MG90S micro servo | Claw actuator |
| 1 | Chassis + claw linkage | 3D-printed or laser-cut |
| — | Jumper wires, Dupont leads, battery connector | Wiring |

---

## 11. Status and next steps

### Working and tested

* Both joysticks read cleanly across their full range (see `GUIDE.md` section 9, Test 1).
* The Bluetooth link establishes, and re-establishes automatically after a dropout.
* All eleven movement commands and all three claw states decode correctly — verified by a
  behavioural test harness that compiles the actual sketches and asserts the resulting pin states.
* Message framing is robust: 500 packets delivered one byte at a time decode with zero errors.
* The 500 ms failsafe stops the robot when the link dies.
* The claw holds its grip without being re-commanded.

### Outstanding (worth doing before the marked run)

| Item | Why it matters | Effort |
| --- | --- | --- |
| **Move the `ENB` wire to D11** | without it the right wheel can't be slowed, so turns are lopsided | 1 wire |
| **Confirm the ENA/ENB jumpers are off** the L298N | otherwise the Nano's enable pins are ignored (and shorted to 5 V) | 2 minutes |
| **Replace `C1`/`C2` with 0.1 µF** if 0.1 F was actually fitted | a supercapacitor across a motor is a huge inrush at power-on | 10 minutes |
| **Fit the 3 A fuse** | a LiPo can deliver enough current to start a fire | 10 minutes |
| **Fit a LiPo low-voltage alarm** | protects an expensive pack from permanent damage | 5 minutes |
| Add `C5` at the servo | removes brown-out resets during hard grabs | 10 minutes |

### Ideas if there is time

* **Proportional control** — map stick deflection to `analogWrite` instead of thresholds, so the
  robot can creep. This is the single biggest upgrade to drivability.
* **Drive and steer together** — blend both axes instead of choosing one.
* **Wheel encoders** — makes repeatable distances and turns possible, and opens the door to
  assisted driving.
* **Battery telemetry** — a divider into a spare analog pin, sent back as a third character.

---

## 12. Document map

| File | Audience | What it answers |
| --- | --- | --- |
| **`OVERVIEW.md`** (this file) | Everyone, especially the operator | What is it, what can it do, how do I drive it |
| **`GUIDE.md`** | Anyone who has to read, modify or debug the code | Every line of all three sketches, plus flashing and first-run procedures |
| **`README.md`** | Quick reference | Pin tables, message protocol, safety notes |
| **`hardware/README.md`** | Anyone building or rewiring | What changed from REV 1.0 and why |
| **`hardware/schematic_rev1.1.png`** | Anyone building or rewiring | The corrected wiring diagram |
| `joystick_serial_monitor_test.ino` | Testing | Proves the input hardware works |
| `controller_code_ESP32.ino` | The controller | Reads sticks, sends commands |
| `arduino_nano_robot_RX_TX.ino` | The robot | Decodes commands, drives motors and claw |

