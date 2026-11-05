# The Complete Beginner's Guide

**Pick & Place Robot — REC Task 2 — Group 5**

This document explains, in plain English, **every part of the three Arduino programs** in this
repository, and then walks you through **putting those programs onto the physical boards** once
the robot is built. No prior Arduino experience is assumed.

Keep these open next to it:

* **`hardware/schematic_rev1.1.png`** — the wiring diagram. Every pin name below matches it.
* **`README.md`** — the short reference sheet (pin tables, message format, safety notes).

---

## Contents

1. [The 30-second version](#1-the-30-second-version)
2. [The big picture — who talks to whom](#2-the-big-picture--who-talks-to-whom)
3. [The three files](#3-the-three-files)
4. [File 1 — the joystick test (start here)](#4-file-1--the-joystick-test-start-here)
5. [File 2 — the controller (ESP32)](#5-file-2--the-controller-esp32)
6. [File 3 — the robot (Arduino Nano)](#6-file-3--the-robot-arduino-nano)
7. [The 8 connections that must be right](#7-the-8-connections-that-must-be-right)
8. [Putting the code on the boards](#8-putting-the-code-on-the-boards)
9. [First power-up — the bench test](#9-first-power-up--the-bench-test)
10. [Troubleshooting](#10-troubleshooting)
11. [The knobs you are allowed to turn](#11-the-knobs-you-are-allowed-to-turn)
12. [Glossary](#12-glossary)

> **Just want to know how to drive it?** Jump straight to
> **[section 5, Part 9 — the gesture-to-motion cheat sheet](#part-9--the-gesture-to-motion-cheat-sheet)**,
> which shows every stick movement and what the robot does.

---

## 1. The 30-second version

The robot has **two brains**, not one.

* A **handheld controller** — an ESP32 board with two joysticks. This is the "remote".
* A **robot** — an Arduino Nano that drives two motors and a claw.

They are not connected by a wire. The controller shouts instructions over **Bluetooth**, and the
robot listens and obeys. Ten times every second the controller sends a **two-letter word**, for
example:

```
FO        "Go Forward, and Open the claw"
SN        "Stop, and leave the claw alone"
```

That is genuinely the whole design. Everything else in this guide is detail about how those two
letters are made, sent, received, understood, and turned into spinning motors.

---

## 2. The big picture — who talks to whom

```
   ┌─────────────────────────┐                      ┌──────────────────────────┐
   │  CONTROLLER (your hands)│                      │   ROBOT (on the floor)   │
   │                         │                      │                          │
   │  2 x joysticks          │   Bluetooth (radio)  │   HC-05 Bluetooth module │
   │      │                  │  ═══════════════►    │      │  (wired)          │
   │      ▼                  │   "FO" 10x/second    │      ▼                   │
   │   ESP32  ───────────────┼──►  no wires  ──────►│   Arduino Nano           │
   │                         │                      │      │                   │
   └─────────────────────────┘                      │      ├──► L298N ──► 2 motors
                                                    │      └──► servo  ──► claw
                                                    └──────────────────────────┘
```

Three things are worth knowing before you read any code:

**a) Bluetooth has a boss.** Two Bluetooth devices cannot both wait for the other to call. One
must be the **master** (it starts the connection) and the other the **slave** (it waits to be
found). Here the **ESP32 is the master** and the **HC-05 is the slave**. So if the robot is off
when you switch the controller on, the controller has nobody to call — it will keep redialling
every 2 seconds until the robot appears.

**b) The HC-05 is a translator, not a brain.** It is a radio-to-wire converter. Bluetooth goes in
one side; ordinary serial characters come out the other side into the Nano's RX pin. It never
understands "forward" — it just carries letters.

**c) The Nano is the only thing that touches hardware.** The ESP32 never knows what a motor is.
It only decides *which letter to send*. All the electrical decisions (which pin goes high, how
fast to spin, what angle the claw sits at) live in the Nano's code.

This separation is why you can debug the halves separately — and section 9 will show you how.

---

## 3. The three files

| File | Goes on | Its job | When you use it |
| --- | --- | --- | --- |
| `joystick_serial_monitor_test.ino` | ESP32 | Prints joystick numbers to your laptop. No Bluetooth, no robot. | **First**, to prove the joysticks and wiring work. |
| `controller_code_ESP32.ino` | ESP32 | Reads the joysticks and sends the two-letter words over Bluetooth. | Normal operation — the handheld remote. |
| `arduino_nano_robot_RX_TX.ino` | Arduino Nano | Receives the words, drives the L298N motor driver and the claw servo. | Normal operation — the robot. |

Note the file extension: **`.ino`**, not `.cpp`. Arduino calls its programs "sketches".

---

## 4. File 1 — the joystick test (start here)

**What it does:** reads both joysticks and prints their numbers to your laptop screen.
That's all. It never talks to the robot.

**Why it exists:** if something feels wrong later ("the robot only goes left"), you can come back
here and instantly know whether the problem is the joystick or the robot. It splits the system in
half. Always run this first after wiring.

### The five words you need

| Word | What it really means |
| --- | --- |
| **sketch** | An Arduino program (a `.ino` file). |
| **pin** | One metal leg on the board. Each one can be an input (listen) or an output (act). |
| **`setup()`** | A block that runs **once**, when the board powers on. Analog: opening the shop in the morning. |
| **`loop()`** | A block that runs **forever**, over and over, thousands of times per second. Analog: serving customers all day. |
| **`Serial`** | The USB cable's text channel to your laptop. Used only for printing messages you read. |

### Line by line

```cpp
// Joystick 1 (Left) Pins
const int J1_VRX = 32;
const int J1_VRY = 33;
const int J1_SW  = 25;
```

`const int J1_VRX = 32;` creates a **nickname**. From now on, `J1_VRX` means "pin 32".
`const` means the nickname can never be changed — a safety label, so you can't accidentally
reassign a pin halfway through the program.

The `32`, `33`, `25` are **GPIO numbers** printed on the ESP32 board, not physical counting
positions. On the schematic you'll see them written as `IO32`, `IO33`, `IO25` — same thing.

Why these particular pins? Because the joystick's X/Y outputs are **analog** (they produce a
varying voltage, not just on/off) and only certain ESP32 pins can measure that. Pins 32–35 are
the analog-capable ones, and they happen to be the only analog pins that still work reliably
while Bluetooth is running (the others are shared with the radio and give nonsense readings).

```cpp
void setup() {
  Serial.begin(115200);
```

`Serial.begin(115200)` opens the text channel and sets its speed to **115200 bits per second**.
This number is *not* optional — when you open the Serial Monitor on your laptop you must set the
same speed, or you'll see scrambled symbols. Think of it as agreeing on a language speed.

```cpp
  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);
}
```

`pinMode(pin, mode)` tells the board what a pin is for.

`INPUT_PULLUP` needs a moment. A button is just a switch that connects a wire to ground when you
press it. But when the button is *not* pressed, that wire is connected to **nothing** — it floats,
picking up random electrical noise, and the board can't tell if it's pressed or not.

`INPUT_PULLUP` fixes this by switching on a tiny resistor **inside** the chip that gently pulls
the wire up to 5 V. Now:

* button **not** pressed → wire sits at **HIGH** (5 V)
* button **pressed** → wire is yanked down to **LOW** (0 V)

So "pressed" means **LOW**. That is the opposite of what most people expect, and it's why you'll
see `== LOW` everywhere in these programs. It also means you need **no extra resistors** — the
schematic correctly shows the joystick `SW` pins wired straight to the board.

```cpp
void loop() {
  int j1_x = analogRead(J1_VRX);
  int j1_y = analogRead(J1_VRY);
  int j2_x = analogRead(J2_VRX);
  int j2_y = analogRead(J2_VRY);
```

`analogRead(pin)` measures the voltage on a pin and converts it to a whole number from **0 to
4095** (the ESP32's analog-to-digital converter is 12-bit, i.e. 2¹² = 4096 steps).

The joystick is really two volume knobs. Centred, each knob sits at half voltage, so the number is
about **2048**. Pushed fully one way it reaches near **0**; fully the other way, near **4095**. So
the number tells you both *which way* and *how far* the stick is leaning.

`int j1_x = ...` stores that number in a box called `j1_x` so the lines below can use it.

```cpp
  bool j1_pressed = digitalRead(J1_SW) == LOW;
```

`digitalRead(pin)` asks "is this pin high or low?" — no middle values, just HIGH or LOW.

`== LOW` is a **question**, not a command: it produces `true` or `false`. So the whole line reads
"is the switch pin low? Store that answer as a true/false value in `j1_pressed`." (`bool` means a
true/false value.)

Note the double `==`. A single `=` means "put this value in the box"; a double `==` means "is it
equal to?". Mixing them up is the most common beginner bug in C++.

```cpp
  Serial.print("LEFT  [X: "); Serial.print(j1_x);
  Serial.print(", Btn: "); Serial.print(j1_pressed ? "PRESSED" : "OPEN");
```

`Serial.print()` sends text to your laptop **without** starting a new line; `Serial.println()`
sends it **and** starts a new line. That is the only difference. Mixing them lets you build one
long line out of many pieces — which is exactly what happens here, ending with `println` so each
reading lands on its own row.

`j1_pressed ? "PRESSED" : "OPEN"` is shorthand for "if pressed show PRESSED, otherwise show OPEN".

```cpp
  delay(100);
}
```

`delay(100)` means "do nothing for 100 milliseconds". As the last thing in `loop()`, this makes
the whole thing repeat about **10 times per second**. Fast for a human, slow for a computer — and
slow enough that the screen doesn't scroll too fast to read.

### What you should see

Open **Tools ▸ Serial Monitor**, set **115200 baud**, and you should get rows like:

```
LEFT  [X: 2048, Y: 2051, Btn: OPEN] | RIGHT [X: 2049, Y: 2047, Btn: OPEN]
```

Push the left stick forward and watch `X` climb toward 4095. Press the stick's button and watch
`OPEN` become `PRESSED`.

**Reading the numbers is a diagnostic in itself:**

| What you see | What it means |
| --- | --- |
| Both axes near **2048** at rest | Perfect — joysticks are healthy. |
| An axis stuck at **0** or **4095** and never moves | Signal wire not connected, or that stick's `Vcc`/`GND` is missing. |
| Both ends of the range are wrong (never reaches 0 / 4095) | Powered from 5 V instead of 3.3 V — see the warning in section 7. |
| Numbers jitter by ±30 while untouched | Normal. Real hardware is noisy, which is exactly why the controller uses a large "dead zone". |
| Nothing at all | Wrong COM port, wrong baud rate, or a charge-only USB cable (no data wires). |


---

## 5. File 2 — the controller (ESP32)

**What it does:** reads both joysticks, turns the readings into **one of eleven letters**, adds a
claw letter, and sends the pair over Bluetooth ten times a second.

It is the file you flash into the board you hold in your hands.

### Three new words

| Word | What it means here |
| --- | --- |
| **master / slave** | In Bluetooth, the master *starts* the connection; the slave *waits*. The ESP32 is the master, the HC-05 is the slave. |
| **dead zone** | A band around the centre where the stick counts as "not pushed". It stops tiny hand tremors and electrical noise from producing movement. |
| **packet / frame** | One complete message. Here: two letters plus a newline, e.g. `FO\n`. |

### Part 1 — turning the Bluetooth feature on

```cpp
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED)
#error "Bluetooth support is missing from this ESP32 build ..."
#elif !defined(CONFIG_BLUEDROID_ENABLED)
#error "This sketch needs the Bluedroid stack (Classic BT / SPP) ..."
#endif
```

`#include` pulls in the Bluetooth feature. It isn't built into the base language; the ESP32 board
package supplies it.

The `#if ... #error ... #endif` block is a **building inspector**. It runs *while the program is
being compiled*, before anything reaches the board. If you pick the wrong kind of ESP32 in the
Arduino IDE (for example an ESP32-C3, which only has the newer "NimBLE" Bluetooth stack), the
compiler refuses and prints a clear sentence instead of letting you upload a program that will
fail mysteriously at three in the morning.

It costs nothing at runtime and can save an hour of head-scratching.

```cpp
BluetoothSerial SerialBT;
```

This creates the object that represents the Bluetooth link — a pipe you can print into and read
from. The name `SerialBT` is the team's choice ("Serial BlueTooth"). `Serial` is the USB pipe;
`SerialBT` is the radio pipe. They are completely separate channels.

```cpp
const char* BT_SLAVE_NAME = "HC-05";
```

The **name the robot's HC-05 broadcasts**. This must match, character for character, or the
controller will scan forever and never find it.

`HC-05` is the factory default. If you ever rename the module with an `AT+NAME` command, change
this string too — the original code had a comment claiming the name was `"Group 5"` while the
code searched for `"HC-05"`, which is exactly the kind of mismatch that wastes an afternoon.

### Part 2 — the dead zone

```cpp
const int AXIS_LOW  = 1000;
const int AXIS_HIGH = 3000;

inline bool pushedLow(int v)  { return v <  AXIS_LOW;  }
inline bool pushedHigh(int v) { return v >  AXIS_HIGH; }
inline bool isNeutral(int v)  { return v >= AXIS_LOW && v <= AXIS_HIGH; }
```

Remember the joystick rests near **2048** and reaches **0** at one extreme and **4095** at the
other. These three helpers turn those raw numbers into plain yes/no questions:

| Function | Reads as | True when |
| --- | --- | --- |
| `pushedLow(x)` | "pushed to the low side?" | below 1000 |
| `pushedHigh(x)` | "pushed to the high side?" | above 3000 |
| `isNeutral(x)` | "left alone?" | between 1000 and 3000 |

Picture the stick's travel as a ruler from 0 to 4095:

```
 0 ────────── 1000 ═══════════════ 3000 ────────── 4095
   pushed LOW   │     neutral      │    pushed HIGH
                └── you must move past this point ──┘
```

Two design decisions hide in there and both are deliberate:

**The dead zone is enormous — you must move the stick about a quarter of its travel before
anything happens.** That sounds sluggish, but it is what makes the robot controllable: a small
dead zone means the robot twitches every time your thumb relaxes.

**Each axis is tested on its own.** An earlier version asked "is the whole other joystick
centred?" before allowing a single-motor command. The practical result was that resting your
finger on the right stick (which nudged its Y axis out of the window) silently cancelled the left
stick's command, and the robot just stopped for no visible reason. Testing `j2_x` alone fixes it.

`inline` is just a speed hint to the compiler: "paste this tiny function in directly instead of
jumping to it". Harmless to ignore.


### Part 3 — `setup()` and the connection routine

```cpp
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Controller", true);
  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);
  Serial.println("ESP32 Controller Ready. Searching for HC-05...");
  attemptConnect();
}
```

* `Serial.begin(115200)` — opens the **USB** channel so you can watch what is happening on your
  laptop. 115200 must match the Serial Monitor setting.
* `SerialBT.begin("ESP32_Controller", true)` — turns on Bluetooth and gives **this** board the
  name `ESP32_Controller`. The `true` is the important part: it means **master mode**, i.e. "I will
  go looking for the robot, it does not have to come looking for me". With `false` the ESP32 would
  sit and wait to be connected to, which is the opposite of what a remote control needs.
* `pinMode(..., INPUT_PULLUP)` — arms the two stick buttons, exactly as in File 1.
* `attemptConnect()` — calls the routine below.

```cpp
void attemptConnect() {
  lastConnectAttempt = millis();

  if (SerialBT.connect(String(BT_SLAVE_NAME))) {
    Serial.println("Connected to HC-05 Successfully!");
  } else {
    Serial.println("Failed to connect. Make sure the HC-05 is powered on and in range - retrying.");
  }
}
```

`millis()` is a stopwatch that starts at zero when the board powers on and counts **milliseconds**
ever since. It never goes backwards. Storing it in `lastConnectAttempt` stamps the moment of the
attempt, so `loop()` can later measure how long it has been.

`SerialBT.connect(...)` starts the radio searching for a device with that name and tries to open a
link. It returns `true` on success or `false` on failure, and the `if` reacts accordingly.

`String(BT_SLAVE_NAME)` converts the constant text into the exact type the library expects. It is
a small formality that also makes the call work across different versions of the ESP32 board
package. If your HC-05 has a non-default PIN, the commented line above `SerialBT.setPin("1234")`
in `setup()` is where you'd set it — and it must be called *before* `SerialBT.begin`.

**Why a separate routine instead of doing it inline in `setup()`?** Because `setup()` runs once.
If the robot is switched on twenty seconds after the controller, a one-shot attempt at boot would
fail and the controller would spend the rest of the match shouting into an empty room. Being a
function lets `loop()` call it again and again.

### Part 4 — the redial guard

```cpp
void loop() {
  if (!SerialBT.hasClient()) {
    if (millis() - lastConnectAttempt >= RECONNECT_INTERVAL_MS) {
      attemptConnect();
    }
    return;
  }
```

Read this as: **"If the phone line is dead, try redialling every two seconds, and do not bother
talking until someone answers."**

* `SerialBT.hasClient()` asks "is somebody connected?" The `!` flips it, so the block runs only
  when **nobody** is connected.
* `millis() - lastConnectAttempt >= 2000` is "has it been at least 2 seconds since the last
  attempt?". Subtracting two `millis()` values instead of comparing them directly is the standard
  safe way to do this: it keeps working even when the counter wraps around after 49 days.
* `return;` means "leave `loop()` immediately, skip everything below". Without it the program
  would carry on and start sending joystick data down a link that doesn't exist — wasted work, and
  it would show a pretty serial log while the robot sat dead on the floor.

Because of this guard, the controller reconnects by itself if the robot's battery is swapped, the
robot drives out of range and comes back, or the HC-05 reboots.

### Part 5 — reading the sticks

```cpp
  int j1_x = analogRead(J1_VRX);
  int j1_y = analogRead(J1_VRY);
  int j2_x = analogRead(J2_VRX);
  int j2_y = analogRead(J2_VRY);

  bool j1_pressed = digitalRead(J1_SW) == LOW;
  bool j2_pressed = digitalRead(J2_SW) == LOW;
```

Identical to File 1, and wired the same way in the schematic: `J1` is the left stick
(`IO32`, `IO33`, `IO25`), `J2` is the right stick (`IO34`, `IO35`, `IO26`).

One detail worth knowing: **`IO34` and `IO35` are "input only" pins** — they physically cannot
drive a voltage out. That is fine for joystick signals (which are inputs), but it is also why those
pins have no internal pull-up, and why the joysticks **must** be powered from **3.3 V** and not
5 V. Feeding 5 V into an ESP32 pin destroys it. The schematic is correct on this point — pin 2
(`Vcc`) on both HW-504 modules goes to `+3V3`.


### Part 6 — deciding which letter to send

```cpp
  char direction = 'S'; // Stop by default
  String dirText = "Stop";
```

`char` is a **single character**, written in single quotes: `'S'`. A `String` with a capital S is
different — that's a whole sentence, written in double quotes. This distinction matters a lot
later, so remember: **`'F'` is one letter; `"Forward"` is text.**

`dirText` is a human-readable label. It is used **only** by the printout on your laptop. It never
travels over Bluetooth — the robot only ever sees the single letter.

Note the default: the program **assumes Stop** and only overwrites it if a condition matches. This
"assume the safe answer first" habit is worth copying; it means an unhandled situation stops the
robot rather than leaving the previous command running.

Then comes a chain of `if / else if`. The order is a priority list — the first match wins and all
the rest are skipped:

| Letter | Robot does | You do this |
| --- | --- | --- |
| `S` | Stop | Leave both sticks centred |
| `F` | Forward, both wheels | Push **both** sticks forward |
| `B` | Backward, both wheels | Pull **both** sticks back |
| `T` | Spin in place (left wheel back, right forward) | Left stick back **and** right stick forward |
| `U` | Spin the other way (left forward, right back) | Left stick forward **and** right stick back |
| `W` | Left wheel forward only | Left stick forward, right stick centred |
| `X` | Left wheel backward only | Left stick back, right stick centred |
| `Y` | Right wheel forward only | Right stick forward, left stick centred |
| `Z` | Right wheel backward only | Right stick back, left stick centred |
| `L` | Gentle curve left | Push **either** stick left (Y axis) |
| `R` | Gentle curve right | Push **either** stick right (Y axis) |

Three things that surprise people:

**"Forward" and "left" are not the same axis.** Forward/back lives on the **X** axis of the sticks
and is read first; steering lives on the **Y** axis and is read last. So **you cannot go forward
and steer at the same time** — pushing a stick forward takes priority and the steering axis is
never even looked at. This is a deliberate limitation of a simple on/off controller. If you want
diagonal driving you'd have to rewrite this section to blend the two.

**The order of the branches is load-bearing.** Spin is checked before forward, forward is checked
before single-motor, and steering is last. Move a branch and the behaviour changes.

**`T` and `U` are the same gesture done opposite ways.** The original code sent `'T'` for both
opposite-stick combinations, so both gestures spun the robot the same way and one direction was
simply unreachable. `'U'` was added so each gesture has its own rotation.

The code for the branch looks like this — every line is just "if this helper says yes, set the
letter":

```cpp
  if (pushedLow(j1_x) && pushedHigh(j2_x)) {
    direction = 'T';
    dirText = "Spin (Left Reverse)";
  } else if (pushedHigh(j1_x) && pushedLow(j2_x)) {
    direction = 'U';
    dirText = "Spin (Left Forward)";
  }
```

`&&` means **and** (both conditions must be true). `||` means **or** (either one is enough — used
in the steering branches so that either stick works).

### Part 7 — the claw

```cpp
  char claw = 'N'; // No change
  if (j1_pressed) {
    claw = 'O';    // Open claw (Joystick 1 button)
  } else if (j2_pressed) {
    claw = 'C';    // Close claw (Joystick 2 button)
  }
```

| Letter | Robot does |
| --- | --- |
| `O` | Open the claw |
| `C` | Close the claw |
| `N` | **No change** — hold whatever position it's in |

`N` is the clever bit. The claw servo only needs to be told a new angle when something changes;
sending "hold" ten times a second would make it buzz and overheat. So `N` means "ignore me", and
the robot leaves the servo exactly where it is.

If both buttons are pressed at once, the first `if` wins and the claw opens. Harmless.

### Part 8 — sending the message

```cpp
  Serial.print("] | Dir: "); Serial.print(dirText);
  Serial.print(" --> Packet: ");
  Serial.print(direction); Serial.println(claw);
```

Everything above this line goes to the **laptop**. It is purely for your eyes, and it is what you
read in the Serial Monitor to understand what the controller has decided. Note that printing
`direction` (a `char`) shows the **letter**, not a number — that's the difference between `'F'` and
`70`.

```cpp
  SerialBT.printf("%c%c\n", direction, claw);
```

**This is the actual transmission.** The whole remote control comes down to this one line.

`printf` means "print formatted". It is a template where `%c` means "put a single character here":

```
"%c%c\n"   with  direction='F', claw='O'   becomes the bytes   F O ⏎
 │ │ │
 │ │ └── \n = a single "newline" character
 │ └──── the claw letter
 └────── the direction letter
```

So the robot receives three bytes: `F`, `O`, and a newline. The **newline is not decoration** — it
is the full stop that tells the Nano where one message ends and the next begins. Characters arrive
over Bluetooth one at a time, sometimes split awkwardly across time, so without a terminator the
receiver could never be sure a message was complete or where the next one starts. Drop the `\n`
and the robot receives nothing at all.

Why one `printf` instead of two prints? Because it's written to the radio in a single operation,
so the two letters can't be interleaved with anything else.

```cpp
  delay(SEND_INTERVAL_MS); // 100 ms
}
```

Ten messages per second. This rate is a compromise: fast enough that the robot responds promptly
when you move a stick, slow enough that the Bluetooth link is never overwhelmed.

It also does a second job — it is the **heartbeat**. The robot uses "have I heard anything
lately?" to decide whether the link is still alive. See section 6, Part 4.


### Part 9 — The gesture-to-motion cheat sheet

This part is not about code. It is about **your hands**. For every way you can move the sticks, here
is the letter that gets sent and what the robot physically does.

#### How to read the doodles

| Symbol | Means |
| --- | --- |
| `^` | push the stick **away** from you (forward) |
| `v` | pull the stick **toward** you (back) |
| `<` / `>` | push the stick **left** / **right** |
| `o` | leave the stick **centred and untouched** |
| `[CLICK]` | press the top of the stick down |

"**Nose**" below means the front of the robot — the end with the claw.

#### Driving

```text
   F  FORWARD                          B  BACKWARD

   +-----+    +-----+                  +-----+    +-----+
   |  ^  |    |  ^  |                  |  v  |    |  v  |
   +-----+    +-----+                  +-----+    +-----+
    left       right                   left       right

   Both wheels roll forward.           Both wheels roll backward.
   The robot drives straight ahead.    The robot reverses straight back.
```

```text
   L  CURVE LEFT                       R  CURVE RIGHT

   +-----+    +-----+                  +-----+    +-----+
   |  <  |    |  o  |                  |  >  |    |  o  |
   +-----+    +-----+                  +-----+    +-----+
    left       right                   left       right

   The left wheel slows down, so       The right wheel slows down, so
   the robot arcs to the left.         the robot arcs to the right.
   (Either stick works - you can       (Either stick works - you can
    use the right one instead.)         use the right one instead.)
```

Note that steering is **not** "turn the front wheels". There is no steering linkage at all — the
robot turns purely because one side of it travels further than the other. That is why a "curve"
command behaves differently from a "spin" command.

#### Fine positioning — one wheel at a time

These four are the most useful commands in the whole set, because they let you rotate the robot a
few degrees without it driving away from the object.

```text
   W  LEFT WHEEL, FORWARD              X  LEFT WHEEL, BACKWARD

   +-----+    +-----+                  +-----+    +-----+
   |  ^  |    |  o  |                  |  v  |    |  o  |
   +-----+    +-----+                  +-----+    +-----+
    left       right                   left       right

   The right wheel is off and free     The right wheel is off and free
   to slide. The robot pivots on       to slide. The robot pivots on
   that wheel:                         that wheel:
   NOSE SWINGS RIGHT                   NOSE SWINGS LEFT
```

```text
   Y  RIGHT WHEEL, FORWARD             Z  RIGHT WHEEL, BACKWARD

   +-----+    +-----+                  +-----+    +-----+
   |  o  |    |  ^  |                  |  o  |    |  v  |
   +-----+    +-----+                  +-----+    +-----+
    left       right                   left       right

   The left wheel is off and free      The left wheel is off and free
   to slide. The robot pivots on       to slide. The robot pivots on
   that wheel:                         that wheel:
   NOSE SWINGS LEFT                    NOSE SWINGS RIGHT
```

> Careful — this is the bit that trips everybody up. **"Left wheel forward" turns the robot
> right**, the same way a tank with its left track running and its right track stopped pivots to
> the right. It is not a bug and it is not backwards; it is just what happens when one side of a
> skid-steer chassis outruns the other.

#### Spinning on the spot

```text
   T  SPIN                             U  SPIN

   +-----+    +-----+                  +-----+    +-----+
   |  v  |    |  ^  |                  |  ^  |    |  v  |
   +-----+    +-----+                  +-----+    +-----+
    left       right                   left       right

   Both wheels are powered, in         Both wheels are powered, in
   opposite directions.                opposite directions.
   NOSE SWINGS LEFT                    NOSE SWINGS RIGHT
```

`T` and `U` are the only two commands where both wheels are driven in opposite directions, so the
robot rotates around its own centre without moving.

#### The claw

```text
   O  OPEN THE CLAW                    C  CLOSE THE CLAW

   +-----+    +-----+                  +-----+    +-----+
   |  o  |    |  o  |                  |  o  |    |  o  |
   +-----+    +-----+                  +-----+    +-----+
   [CLICK]                             [CLICK]
    left       right                   left       right

   Press the LEFT stick's top.         Press the RIGHT stick's top.
   Jaws open.                          Jaws close.
```

**You do not have to hold the button.** The servo travels to the new angle and stays there. A quick
tap is enough — press, release, and the claw holds. Holding the button down just re-sends the same
angle over and over.


#### The complete reference

All fourteen possible states. `●` means the stick is centred and untouched.

| You do this | Letter | Left wheel | Right wheel | Nose swings | In one line |
| --- | --- | --- | --- | --- | --- |
| both sticks `^` | `F` | forward | forward | — | drives straight ahead |
| both sticks `v` | `B` | backward | backward | — | reverses straight back |
| either stick `<` | `L` | slow forward | full forward | left | arcs left while moving |
| either stick `>` | `R` | full forward | slow forward | right | arcs right while moving |
| left `^`, right `●` | `W` | forward | off | **right** | pivots on the right wheel |
| left `v`, right `●` | `X` | backward | off | **left** | pivots on the right wheel |
| right `^`, left `●` | `Y` | off | forward | **left** | pivots on the left wheel |
| right `v`, left `●` | `Z` | off | backward | **right** | pivots on the left wheel |
| left `v`, right `^` | `T` | backward | forward | left | spins on the spot |
| left `^`, right `v` | `U` | forward | backward | right | spins on the spot |
| `[CLICK]` left stick | `O` | (unchanged) | (unchanged) | — | claw opens |
| `[CLICK]` right stick | `C` | (unchanged) | (unchanged) | — | claw closes |
| nothing touched | `S` | off | off | — | stops |
| anything unrecognised | — | off | off | — | fails safe, stops |

#### What it actually feels like to drive

Five things you will notice in the first minute, and none of them are faults:

**1. There is a short lag.** About a tenth of a second between your thumb moving and the wheels
reacting. It feels a little like driving on ice. Learn to lead the robot slightly and stop a
moment before you think you need to.

**2. Nothing happens until you push about a quarter of the way.** That is the dead zone. It is what
stops the robot twitching every time your thumb relaxes. Push decisively rather than gently.

**3. Letting go means stop.** The sticks are spring-loaded and centre themselves, which sends `S`.
You never need a separate "brake" gesture.

**4. It coasts.** There are no brakes, only "stop powering the motors". At full speed on a smooth
floor the robot will roll on for a short distance. On a low-friction surface, expect more.

**5. One stick forward does not mean "go forward".** This is the number-one beginner surprise: the
robot only drives straight when **both** sticks are pushed the same way. Pushing one stick gives you
`W`, `X`, `Y` or `Z` — a pivot on one wheel — and the robot will rotate instead of advancing.

#### The five mistakes everybody makes first

| What you try | What actually happens | Do this instead |
| --- | --- | --- |
| Push the left stick forward and expect it to go forward | `W` — it pivots on the right wheel and swings right | push **both** sticks forward |
| Push a stick forward-and-to-the-side for a curve | the sideways part is ignored — steering is only read when X is centred | drive straight, stop, then steer |
| Hold the claw button down for a second "to make sure" | pointless — the angle is already set | tap it once |
| Expect it to stop dead | it coasts | stop earlier than feels natural |
| Lean a finger on the other stick | a light touch is fine (dead zone), a firm one cancels your command | keep the other hand clear |


---

## 6. File 3 — the robot (Arduino Nano)

**What it does:** listens on its serial port for two letters, and turns them into motor directions,
motor speeds, and a claw angle.

This is the file that actually touches the hardware, so it is the one that can break the robot.
Read the pin section before you solder anything.

### Six new words

| Word | What it means here |
| --- | --- |
| **PWM** | Switching a pin on and off very fast. The *fraction* of time it is on is the "duty cycle", which controls motor speed. A light dimmer, in digital form. |
| **H-bridge** | A circuit that can push current through a motor in *either* direction. The L298N contains two of them. |
| **framing** | The rules for where a message starts and ends on a stream of characters. |
| **buffer** | A small holding area where arriving bytes wait to be read, like an in-tray. |
| **watchdog / failsafe** | A rule that acts when something that *should* happen stops happening. |
| **`millis()`** | A millisecond stopwatch, running since power-on. |

### Part 1 — the pins (read this before wiring)

```cpp
const int ENA = 6;  // PWM speed pin for Motor 1 (Left)  - Timer0
const int IN1 = 5;
const int IN2 = 4;
const int IN3 = 3;
const int IN4 = 2;
const int ENB = 11; // PWM speed pin for Motor 2 (Right) - Timer2A
```

Notice that `ENB` is **11**. This is the single most important number in the whole project, because
two earlier versions got it wrong:

| Pin | PWM capable? | Verdict |
| --- | --- | --- |
| **D7** (the original code) | **No** — only D3, D5, D6, D9, D10, D11 are | `analogWrite` silently becomes on/off. "Slow turn" became "wheel stopped". |
| **D10** (the original schematic) | Yes, **but** the Servo library takes over Timer1 for the claw | `analogWrite` produces a 0.5 % duty cycle — the wheel effectively never turns |
| **D11** (correct) | Yes, on Timer2A, which nothing else uses | Works |

`ENB` must also be physically moved to D11 on the robot. See section 7.

The other five are ordinary direction pins, so any pin works — they happen to be D5, D4, D3, D2
because that is how the board was wired. The claw servo takes D9.

**Why does the servo care which pin it's on?** Inside the Arduino there are three little
stopwatches called *timers* that generate the fast switching for PWM. The Servo library grabs one
of them (Timer1) and uses it in a completely different mode to produce the 50-times-per-second
pulses a servo needs. Timer1 also controls pins 9 and 10. So while a servo is attached, D9 and D10
stop being usable as motor-speed pins. D11 uses Timer2A, which the servo leaves alone.

### Part 2 — speed and claw settings

```cpp
const int motorSpeed = 200;
const int turnSpeed = 80;

const int CLAW_NEUTRAL_DEG = 90;
const int CLAW_OPEN_DEG    = 160;
const int CLAW_CLOSE_DEG   = 20;
```

Speeds are **0 to 255**. This is the PWM duty cycle: `200` means the power is switched on about
78 % of the time, so the motor runs at roughly three-quarter speed. `0` is off, `255` is fully on.

`motorSpeed` is the normal speed for both wheels. `turnSpeed` is the speed of the **inner** wheel
during a turn — the wheel on the inside of the curve runs slower so the robot arcs instead of
skidding. (It was a hard-coded `80` in the original; naming it makes the symmetry between left and
right turns visible.)

Claw angles are in **degrees**, 0 to 180, as the servo understands them. `90` is the mechanical
middle the claw starts in; `160` opens it, `20` closes it. If your claw is assembled the other way
round, just swap the 160 and the 20.


### Part 3 — the watchdog and the in-tray

```cpp
const unsigned long LINK_TIMEOUT_MS = 500;
unsigned long lastPacketMs = 0;

#define DEBUG_SERIAL 0

char    frameBuf[8];
uint8_t frameLen = 0;
```

`LINK_TIMEOUT_MS = 500` and `lastPacketMs` together form the **dead-man's switch**. Think of a
train driver holding a handle: if the driver lets go, the train stops. Here, the controller sends
something every 100 ms; if the robot hears **nothing for 500 ms** it stops the motors.

This is not a nicety. Without it, if the Bluetooth link drops (out of range, battery flat, ESP32
crashed) the Nano keeps obeying the **last** command forever and the robot drives into a wall at
full speed. It is the most important safety feature in the project, and it costs nine lines.

`DEBUG_SERIAL` is a **switch for the programmer**. Set it to `1` and the robot prints every message
it receives over USB — useful while debugging. It is set to `0` for driving because printing is
slow: at 9600 baud each printed line blocks the program for about 50 ms, which is half the time
between messages. Leaving it on makes the robot laggy and can cause dropped messages.

`frameBuf[8]` is the **in-tray**: a row of 8 boxes where incoming characters wait. `frameLen` is a
counter of how many are currently in it. Eight is far more than the two we need — the extra room
means a corrupted burst can't overflow it.

### Part 4 — `setup()`

```cpp
void setup() {
  Serial.begin(9600);
  delay(1000);
  ...
}
```

**`9600`, not `115200`.** This is the speed the HC-05 talks at out of the box. If you ever
reconfigure the HC-05 to a different speed, this number must change to match. Getting this wrong is
the classic "I see garbage characters" bug.

`delay(1000)` waits one second at startup. The HC-05 has its own little computer inside and needs a
moment to boot. Sending data before it's ready loses the first few messages.

```cpp
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);  pinMode(IN4, OUTPUT);

  clawServo.attach(9);
  clawServo.write(CLAW_NEUTRAL_DEG);
```

Every pin that will *drive* something must be declared an `OUTPUT` first, otherwise it stays in a
high-impedance "listening" state and the motor driver sees nothing.

`clawServo.attach(9)` tells the Servo library which pin the servo's signal wire is on, and starts
the pulse generation. `clawServo.write(90)` sends it to the middle so the claw doesn't slam to one
end at power-on.

```cpp
  lastPacketMs = millis();
}
```

Stamps "the link last worked *now*", so the watchdog doesn't immediately fire before the first
message has had a chance to arrive. The motors stay stopped until that first message — which is the
correct way round.

### Part 5 — `loop()`: two jobs

```cpp
void loop() {
  serviceLink();

  if (millis() - lastPacketMs > LINK_TIMEOUT_MS) {
    stopMotors();
  }
}
```

The entire main loop is two lines of work:

1. **`serviceLink()`** — read whatever has arrived and act on it (Part 6).
2. **The watchdog** — if the last valid message is more than 500 ms old, stop.

Note what the loop does **not** do: it never waits for anything. It runs thousands of times per
second, checking, checking, checking. That is why a half-arrived message can never freeze the
robot — the program pockets whatever bytes exist and moves on to the next lap.

Asking `millis() - lastPacketMs > 500` every lap also means the motors are commanded to stop over
and over while the link is down. That's intentional: re-sending "stop" is free, and it makes it
impossible for a stale "forward" command to linger.

### Part 6 — `serviceLink()`: turning a stream of bytes into messages

This is the most subtle part of the whole project, so read it slowly.

```cpp
void serviceLink() {
  while (Serial.available() > 0) {
    char c = Serial.read();
```

* `Serial.available()` = "how many bytes are sitting in the in-tray right now?" Zero means nothing
  has arrived; nothing to do.
* `while (... > 0)` keeps going until the tray is empty.
* `Serial.read()` takes **one** byte out and stores it in `c`. This is why messages must be
  reassembled: the Nano receives characters one at a time, not as tidy two-letter parcels.

```cpp
    if (c == '\n' || c == '\r') {
      if (frameLen >= 2) {
        applyCommand(frameBuf[0], frameBuf[1]);
        lastPacketMs = millis();
      }
      frameLen = 0;
    }
```

A newline means **"that's the end of a message"**. When one arrives:

* If we collected **at least two** characters, we have a complete message. Hand the first two to
  `applyCommand()` and stamp the watchdog.
* If we collected fewer than two, the message was cut short (or it was a stray blank line).
  Throw it away — acting on a one-character message would mean guessing.
* Either way, reset `frameLen = 0` so the tray is ready for the next message.

```cpp
    else if (frameLen < sizeof(frameBuf) - 1) {
      frameBuf[frameLen++] = c;
    } else {
      frameLen = 0;   // overflow -> resync
    }
  }
}
```

Normal characters get added to the tray, and the counter goes up (`frameLen++`).

The final `else` is the survival clause: if more than 7 characters arrive without a newline,
something is badly wrong (wrong baud rate, electrical noise, a mis-configured module). Rather than
overflowing the tray, the counter resets and the parser starts fresh at the next newline. One bad
message is discarded instead of poisoning every message after it.

**Why this design replaced the original.** The first version read two bytes at a time and then
tried to sweep up trailing newline characters. In simulation it decoded 56 % of messages
incorrectly when bytes arrived in dribs and drabs, and — worse — the moment anyone added a third
character to the message it would lock up permanently, reading every message one character out of
step and never recovering without a power cycle. Looking for a newline is a few extra lines and it
cannot get stuck.


### Part 7 — `applyCommand()`: doing what the letters asked

```cpp
void applyCommand(char dir, char claw) {
#if DEBUG_SERIAL
  Serial.print("Received Packet -> Dir: [");
  ...
#endif
```

`#if DEBUG_SERIAL ... #endif` is a **compile-time** switch, not an `if`. When `DEBUG_SERIAL` is
`0`, the compiler deletes those print lines entirely — they cost nothing at runtime and don't even
exist in the uploaded program. Change the `0` to `1` and they come back.

```cpp
  switch (dir) {
    case 'F': moveForward();        break;
    case 'B': moveBackward();       break;
    case 'L': turnLeft();           break;
    case 'R': turnRight();          break;
    case 'T': spinSpot();           break;
    case 'U': spinSpotCounter();    break;
    case 'W': leftMotorForward();   break;
    case 'X': leftMotorBackward();  break;
    case 'Y': rightMotorForward();  break;
    case 'Z': rightMotorBackward(); break;
    case 'S': stopMotors();         break;
    default:  stopMotors();         break;
  }
```

`switch` is a cleaner way of writing a long `if / else if` chain when you are comparing one
character against a list of known values. It jumps straight to the matching `case`.

`break;` means "stop here, don't fall through to the next case". Forget a `break` and the program
runs the following case as well — a classic bug. Every case here has one.

`default:` catches **anything that isn't in the list** — a corrupted byte, a letter from a future
version, noise on the link. It stops the motors. Stopping is the safe response: the worst outcome
is that the robot pauses for a tenth of a second, not that it lurches in a random direction.

The claw is handled separately, after the movement switch:

```cpp
  if (claw == 'O') {
    clawServo.write(CLAW_OPEN_DEG);
  } else if (claw == 'C') {
    clawServo.write(CLAW_CLOSE_DEG);
  }
```

There is deliberately **no `else`**. If the letter is `N` (or anything unexpected), neither branch
runs and the servo is not commanded at all — it simply holds its current position. Telling a servo
to hold still when it is already holding still just makes it buzz, draw current, and get warm.

### Part 8 — the motor functions and the H-bridge

Each of the eleven commands is a short function. They all work the same way, so here is one:

```cpp
void moveForward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
```

Two things happen for each wheel:

**1. Set the direction** with `IN1`/`IN2` (left wheel) or `IN3`/`IN4` (right wheel). The L298N is
an **H-bridge**: it can push current through the motor either way. Its rules are fixed by its
design:

| Pin A | Pin B | What the motor does |
| --- | --- | --- |
| `HIGH` | `LOW` | spins **forward** |
| `LOW` | `HIGH` | spins **backward** |
| `LOW` | `LOW` | **coasts** — no current, free to be pushed |
| `HIGH` | `HIGH` | **brakes** — both terminals connected, hard to turn |

**2. Set the speed** with `analogWrite(ENA, ...)`. `analogWrite` produces PWM: it flips the pin on
and off rapidly, and the value 0–255 sets what fraction of the time it is on. `analogWrite(x, 0)`
is fully off, so the "only one wheel" commands simply switch the other wheel's enable to zero.

So the eleven commands are:

| Command | Left wheel | Right wheel | Result |
| --- | --- | --- | --- |
| `F` | forward, full speed | forward, full speed | straight ahead |
| `B` | backward, full speed | backward, full speed | straight back |
| `L` | forward, **slow** (`turnSpeed`) | forward, full speed | gentle arc left |
| `R` | forward, full speed | forward, **slow** (`turnSpeed`) | gentle arc right |
| `T` | backward, full speed | forward, full speed | spin in place |
| `U` | forward, full speed | backward, full speed | spin the other way |
| `W` / `X` | forward / backward, full speed | **off** (coasts) | pivot on the right wheel |
| `Y` / `Z` | **off** (coasts) | forward / backward, full speed | pivot on the left wheel |
| `S` | off | off | stopped |

The "only one wheel" commands deliberately **coast** the idle wheel rather than braking it. A
coasting wheel lets that side of the robot slide, so the chassis pivots around it. A braked wheel
would drag, and the turn would be jerky.

This is also why the `ENB` pin matters so much: every row above that says "full speed" or "slow"
relies on `analogWrite` being able to genuinely vary the power. On a non-PWM pin, "slow" silently
becomes "off", which is why the robot used to pivot one way and curve the other.

### Part 9 — `stopMotors()`

```cpp
void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
```

Speed to zero **and** both direction pins low on both channels. The robot coasts to a stop rather
than slamming the brakes on, which avoids the gears taking a beating.

This is the single function that the watchdog calls, the `'S'` command calls, and `default:` calls.
Three different paths, one known-good outcome.


---

## 7. The 8 connections that must be right

Everything else on the schematic is forgiving. These eight are not — each one has caused a real
failure in this project. Work through them with the schematic open and physically verify each one.

### ✅ 1. `ENB` goes to **D11** on the Nano

| | |
| --- | --- |
| **Schematic shows** | the orange `D11 → ENB` wire |
| **Wrong versions** | D10 (on the REV 1.0 drawing) and D7 (in the first code) both fail |
| **If wrong** | the right wheel cannot be slowed down; turns come out asymmetric, `Y`/`Z` behave oddly |
| **Verify** | with the robot commanded forward, a multimeter on `ENB` should read about **3.9 V** (78 % of 5 V), not 5 V and not 0.03 V |

### ✅ 2. Joystick `Vcc` goes to **3V3**, never 5 V

| | |
| --- | --- |
| **Schematic shows** | both HW-504 pin 2 (`Vcc`) on the purple `+3V3` flag |
| **If wrong** | the ESP32 pins `IO34`/`IO35` are **input-only and not 5 V tolerant** — 5 V will destroy them, sometimes immediately |
| **Verify** | measure the voltage on joystick pin 2 with the controller on: it must read ~3.3 V |

### ✅ 3. HC-05 ↔ Nano: cross the wires, and put the divider on the TX side

| | |
| --- | --- |
| **Schematic shows** | `Nano D1/TX → R1 1k → [junction + R2 2k to GND] → HC-05 RX`, and `HC-05 TX → Nano D0/RX` **directly** |
| **Why the crossing** | "TX" means *transmit*. Data must go **out** of one side and **in** to the other, so the Nano's TX must reach the HC-05's RX |
| **Why only one divider** | the Nano transmits at 5 V and the HC-05 expects 3.3 V, so that path needs the divider. Going the other way the HC-05 sends 3.3 V, which the Nano reads happily (anything above 3.0 V counts as HIGH). Dividing *that* path down to 2.2 V would fall below the threshold and the link would become unreliable |
| **If wrong** | no communication at all, or intermittent dropped commands |

### ✅ 4. Every board shares one ground

| | |
| --- | --- |
| **Schematic shows** | the `⏚` symbol on the battery, the buck, the Nano, the L298N, the servo, the HC-05 and both joysticks |
| **Why** | voltage is a *difference*. Without a shared reference, "5 V" on one board means nothing to another board. Missing ground is the number-one cause of "it works when I touch it with my finger" |
| **Verify** | with everything off, a continuity test should beep between any two ground points |

### ✅ 5. L298N: one jumper **off**, one jumper **on**

| | |
| --- | --- |
| **ENA/ENB jumpers** | **REMOVE.** They tie the enable pins permanently to 5 V. If left in while the Nano drives those pins, a pin outputting LOW is shorted straight to 5 V — that can kill the pin |
| **5 V jumper** | **KEEP.** It lets the module's own regulator make its logic supply from the 7.4 V input. With 7.4 V (below the 12 V that would require an external supply) this is the correct setting |
| **If wrong** | motors run at full speed regardless of the code, or the Nano's pins get damaged |

### ✅ 6. Battery → **fuse** → **switch** → the 7.4 V rail

| | |
| --- | --- |
| **Schematic shows** | `BT1 → F1 3 A → S1 → +7.4 V` |
| **Why the fuse** | a 2S LiPo can deliver enormous current into a short circuit. Without a fuse, a wiring mistake can start a fire |
| **Also strongly recommended** | a LiPo low-voltage alarm on the balance lead — a 2S pack taken below ~6.6 V is permanently damaged |

### ✅ 7. The buck's 5 V output feeds the Nano's **5V pin**, not `VIN`

| | |
| --- | --- |
| **Schematic shows** | buck `OUT+` → `+5 V` rail → Nano pin 27 (`5V`) |
| **Why** | `VIN` expects 7–12 V and passes it through the Nano's own regulator, which is inefficient and limited. Feeding a clean, regulated 5 V into the `5V` pin bypasses that stage |
| **Careful** | while the buck is powering that rail, avoid leaving the USB cable plugged into the Nano for long periods — the two 5 V sources are directly connected on this board |

### ✅ 8. Servo signal on **D9**, power from the 5 V rail

| | |
| --- | --- |
| **Schematic shows** | servo `SIG` → `D9`, `V+` → `+5 V`, `GND` → ground |
| **Why not from the Nano's own regulator** | a stalling MG90S pulls around 1 A. That would brown out the Nano and reset it mid-grab. Using the separate buck is what makes the claw reliable |
| **Optional but wise** | a 470 µF capacitor (`C5` on the drawing) right at the servo |

### And one thing to leave unplugged

The HC-05 sits on **D0/D1**, which is the same serial port the USB chip uses. So:

* **Unplug the HC-05 from D0/D1 before uploading** to the Nano, then plug it back in. Otherwise
  its transmit pin fights the USB chip and the upload fails.
* **The robot must run untethered.** With USB plugged in, the USB chip holds the Nano's receive
  line high and the Bluetooth data cannot get through.


---

## 8. Putting the code on the boards

"Uploading" (or "flashing") means copying a `.ino` file from your computer into a chip on the
board, where it is stored permanently. It survives power-off.

You will do this **twice**: once for the ESP32 (the remote) and once for the Nano (the robot).

### Before you start

- [ ] **Arduino IDE 2.x** installed from <https://www.arduino.cc/en/software>
- [ ] A **USB cable that carries data**. Many cables are charge-only, and they look identical.
      If the board never appears in the port list, try a different cable before anything else.
- [ ] The robot **built and wired** per `hardware/schematic_rev1.1.png`, section 7 checked.
- [ ] **The robot switched OFF and the battery disconnected** while you plug USB in.

> **Linux users only:** you need permission to talk to the USB port. Once, in a terminal:
>
> ```bash
> sudo usermod -a -G dialout $USER
> ```
>
> then **log out and back in** (or reboot). Without this the port list stays empty or you get
> "permission denied".

### Step 1 — Add support for the ESP32

The Arduino IDE ships knowing about Arduino-branded boards only. The ESP32 needs a free add-on.

1. Open **File ▸ Preferences** (Linux/Windows) or **Arduino IDE ▸ Settings** (macOS).
2. Find **Additional boards manager URLs** and paste exactly this:

   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

3. Click **OK**.
4. Open the **Boards Manager** (the second icon in the left toolbar, or **Tools ▸ Board ▸ Boards
   Manager**).
5. Search `esp32`, find **esp32 by Espressif Systems**, and click **Install**. This is a large
   download — several hundred megabytes — and it takes a while.
6. When it finishes, **restart the Arduino IDE**. This matters: the new board definitions are only
   picked up on a fresh start.

### Step 2 — Check the Servo library

Nothing to install. **`Servo` comes bundled with the Arduino IDE**, and `BluetoothSerial` comes
with the ESP32 package you just installed. Do **not** install a random "BluetoothSerial" library
from the Library Manager — it will conflict with the built-in one and produce confusing errors.


### Step 3 — Flash the joystick test (ESP32)

Do this **before** the real controller. It proves the joysticks and the wiring are good while there
is nothing else that could be at fault.

1. Connect the ESP32 by USB.
2. **File ▸ Open…** → `joystick_serial_monitor_test.ino`.
3. **Tools ▸ Board ▸ esp32 ▸ ESP32 Dev Module.**
4. **Tools ▸ Port** → pick the one that appeared when you plugged the board in
   (`/dev/ttyUSB0` or `/dev/ttyACM0` on Linux, `COM3`-style on Windows).
5. Leave the other settings alone.
6. Press the **→ Upload** button (top left).
7. Wait for `Done uploading.`
8. Open **Tools ▸ Serial Monitor**, set the speed box (bottom right) to **115200**, and confirm the
   readings behave as described at the end of section 4.

If this step works, the controller's hardware is fine and any later problem is software or
Bluetooth.

### Step 4 — Flash the controller (ESP32)

1. **File ▸ Open…** → `controller_code_ESP32.ino`.
2. Confirm **ESP32 Dev Module** and the correct **Port** are still selected.
3. Upload.
4. Open the Serial Monitor at **115200**. You will see either `Connected to HC-05 Successfully!` or:

   ```
   ESP32 Controller Ready. Searching for HC-05...
   Failed to connect. Make sure the HC-05 is powered on and in range - retrying.
   ```

   The second one is **normal and expected** if the robot is switched off. It keeps retrying every
   two seconds by itself — leave it plugged in.

> **If the upload fails** with `Failed to connect to ESP32: Timed out waiting for packet header`,
> hold down the **BOOT** button on the ESP32 while the `Connecting.....` dots are appearing, then
> release it. Some devkits need this nudge. It is not a fault.

### Step 5 — Flash the robot code (Nano)

**This is the step where people lose an hour, so read it fully first.**

1. **Switch the robot OFF and disconnect the battery.**
2. **Unplug the HC-05's two wires from the Nano's D0 and D1.** Two wires, easy to miss. The HC-05
   shares the Nano's serial port with the USB chip, and if it is connected it will fight the upload
   and the upload will fail with confusing timeout messages.
3. Connect the Nano by USB.
4. **File ▸ Open…** → `arduino_nano_robot_RX_TX.ino`.
5. **Tools ▸ Board ▸ Arduino AVR Boards ▸ Arduino Nano.**
6. **Tools ▸ Processor** → try **ATmega328P**. If the upload later fails with
   `avrdude: stk500_recv(): programmer is not responding`, change it to
   **ATmega328P (Old Bootloader)** and try again. Most low-cost Nano clones need the Old Bootloader.
7. **Tools ▸ Port** → the Nano's port.
8. Upload.
9. **Reconnect the HC-05 to D0 and D1**, the same way round as before. This is critical: swapping
   TX/RX here means the robot will never receive anything.
10. Unplug USB.

> **Never upload to the Nano with the HC-05 still attached.** Even if it occasionally works, the
> collision between the two transmit pins is not good for either chip.

### Step 6 — Power-up order and first run

There is a correct order, and it exists because of how Bluetooth pairing works.

1. **Robot on first.** Flip the robot's switch (battery connected, section 7 checked). The claw
   should twitch to its centre position and the motors should stay still. The HC-05's LED should be
   **blinking fast** — that means "not paired yet".
2. **Controller on second.** Power the ESP32 (USB power bank). Within a few seconds the HC-05's LED
   should go to a **slow double-blink** — that means "paired".
3. Watch the controller's Serial Monitor. It should now say `Connected to HC-05 Successfully!` and
   start showing joystick numbers.
4. **Test with the robot's wheels off the ground**, or the robot on a box, for the first thirty
   seconds.

If you power the controller on first, nothing is broken — it just redials every two seconds until
the robot appears. That is what the reconnect guard in section 5, Part 4 is for.


---

## 9. First power-up — the bench test

Do these in order. Each step tests one link in the chain, so when something fails you know exactly
which link it was. Don't skip ahead "to save time" — this takes six minutes and saves hours.

### Test 1 — joysticks alone (ESP32 on USB, robot off)

| Do | Expect |
| --- | --- |
| Flash `joystick_serial_monitor_test.ino` | `Done uploading.` |
| Open the Serial Monitor at 115200 | rows of `LEFT [X: ...` |
| Leave both sticks alone | all four axes between about 1900 and 2200 |
| Push left stick forward | `X` on the left rises toward 4095 |
| Press left stick's button | `Btn: PRESSED` |
| Do the same for the right stick | its values change, the left ones don't |

### Test 2 — the radio link (robot on, controller on USB)

| Do | Expect |
| --- | --- |
| Flash `controller_code_ESP32.ino` to the ESP32 | `Done uploading.` |
| Power the robot | claw centres, motors still, HC-05 LED blinks fast |
| Watch the controller's Serial Monitor | `Connected to HC-05 Successfully!` within a few seconds |
| Check the HC-05 LED | switches to slow double-blink |

If it still says "Failed to connect" after 10 seconds, the controller is working correctly and the
problem is at the robot end.

### Test 3 — motors, wheels off the ground

Put the robot on a box so the wheels spin in the air. **This is not optional for the first test** —
if a motor is wired backwards you want to find out with nothing to crash into.

| Do | Expect |
| --- | --- |
| Push both sticks forward | both wheels spin the same way (the "forward" way) |
| Push both back | both reverse |
| One wheel forward and the other back | they counter-rotate |
| Push left stick forward only | only the left wheel spins |
| Push right stick forward only | only the right wheel spins |
| Push either stick left | both wheels forward, one slower |
| Release everything | both stop, no buzzing |

Two specific things to check here, because they are what the `ENB` fix was about:

* **Both single-wheel tests must produce the same wheel speed.** If `Y` (right wheel) is much
  weaker or doesn't move at all compared to `W` (left wheel), `ENB` is on the wrong pin — go back
  to section 7, item 1.
* **Left and right curves should look like mirror images.** If one curves gently and the other
  pivots on the spot, that is the same `ENB` problem.

### Test 4 — the claw

| Do | Expect |
| --- | --- |
| Press left stick's button | claw opens |
| Release, wait a second | claw **stays** where it is |
| Press right stick's button | claw closes |
| Release | stays |

If the claw buzzes constantly while idle, the servo is being commanded every cycle — check that
`claw` is `'N'` when no button is pressed.

### Test 5 — the failsafe (do this one, it matters)

1. Drive the robot forward.
2. **While it is moving, switch the robot's battery off, or walk the controller out of range.**
3. Within half a second the robot must **stop**, not keep driving.

If it keeps driving, the watchdog is not working — check `LINK_TIMEOUT_MS` and `loop()` in
section 6, Part 5. This test is the difference between a robot that fails safely and a robot that
drives into a wall.

### Test 6 — real driving

Only now, on the floor, at low speed (`motorSpeed` around 120–150 while you learn), with the
battery charged.


---

## 10. Troubleshooting

| Symptom | Most likely cause | Fix |
| --- | --- | --- |
| The board doesn't appear in **Tools ▸ Port** | charge-only USB cable, missing driver, or Linux permissions | try another cable; on Linux add yourself to `dialout` (section 8) |
| `BluetoothSerial.h: No such file or directory` | ESP32 package not installed, **or the wrong board is selected** (e.g. "Arduino Uno") | install the ESP32 package; select **ESP32 Dev Module** |
| An `#error` about Bluedroid / `CONFIG_BT_ENABLED` | you selected an ESP32-C3 / S2 / H2 — they have no Classic Bluetooth | use a plain ESP32 (WROOM-32) |
| Upload to the ESP32 times out at `Connecting.....` | some devkits need a nudge | hold **BOOT** during the dots, then release |
| Upload to the **Nano** fails, `stk500_recv(): programmer is not responding` | (a) the HC-05 is still plugged into D0/D1, or (b) wrong bootloader | unplug the HC-05; set **Processor ▸ ATmega328P (Old Bootloader)** |
| Serial Monitor shows `??????` or garbage | baud rate mismatch | ESP32 sketches: **115200**. The Nano robot sketch: **9600** |
| Controller says `Failed to connect` forever | HC-05 not powered; no Classic BT on the ESP32; name mismatch; out of range | power the robot first; check `BT_SLAVE_NAME` matches `AT+NAME`; check the HC-05's power LED |
| HC-05 LED blinks fast and never pairs | the module is not a **slave** at **9600**, or it wants a PIN | reconfigure the module; if it demands a PIN, add `SerialBT.setPin("1234")` before `begin()` |
| Connected, but the robot never moves | HC-05 TX/RX swapped; divider on the wrong side; HC-05 left unplugged after upload | section 7, item 3 |
| **Only one wheel** ever turns | `ENB` not on **D11**; ENA/ENB jumper still fitted on the L298N | section 7, items 1 and 5 |
| The right wheel ignores the slow setting; turns look lopsided | `ENB` on a non-PWM pin (D7), or on D10 which the servo timer owns | rewire `ENB` to **D11** |
| Robot spins instead of driving straight | one motor's two wires are swapped | swap that motor's two output wires at the L298N |
| Robot drives **backwards** when you push forward | both motors swapped, **or** the sticks are mounted 180° round | swap both motors' wires, or push the sticks the other way |
| Robot creeps with the sticks untouched | dead zone too small, or an axis is mis-wired | raise `AXIS_LOW` / lower `AXIS_HIGH`; re-run Test 1 |
| Robot pauses for a moment now and then | a brief Bluetooth drop — the watchdog did its job | move closer, check the aerial; the pause is correct behaviour |
| Robot **stops after about half a second, every time** | it is not receiving anything, so the watchdog fires | this is a link problem, not a motor problem — check the HC-05 → Nano RX path |
| The Nano keeps resetting while driving | brown-out: flat battery, or the servo dragging the 5 V rail down | charge the battery; add the 470 µF cap at the servo; check the buck's current rating |
| Claw twitches and the board resets | servo powered from the Nano's regulator instead of the buck | section 7, item 8 |
| Works, then dies after ~20 minutes | the LiPo is below its safe voltage | recharge; fit a LiPo low-voltage alarm |
| Robot reacts slowly, prints look sluggish | `DEBUG_SERIAL` is set to `1` | set it back to `0` |


---

## 11. The knobs you are allowed to turn

Every value below sits at the top of its file with a comment, so you can find it without reading
the whole program. Change **one at a time**, and re-run the bench test after each change.

### In `arduino_nano_robot_RX_TX.ino` (the robot)

| Setting | Default | Raise it | Lower it |
| --- | --- | --- | --- |
| `motorSpeed` | `200` | faster, more wheel slip, battery lasts less | slower, safer while learning. Below ~100 the motors may not overcome friction |
| `turnSpeed` | `80` | wider, faster turns | tighter turns; too low and the inner wheel stalls and it becomes a pivot |
| `CLAW_OPEN_DEG` | `160` | opens further | opens less |
| `CLAW_CLOSE_DEG` | `20` | closes less | squeezes harder (watch for servo stall/buzzing) |
| `LINK_TIMEOUT_MS` | `500` | tolerates longer dropouts, but the robot coasts further if the link dies | stops faster on link loss, but may stop spuriously on a flaky link. Never set it below ~3× the controller's `SEND_INTERVAL_MS` |
| `DEBUG_SERIAL` | `0` | prints every received message — use `1` only while debugging | `0` for driving |

### In `controller_code_ESP32.ino` (the remote)

| Setting | Default | Raise it | Lower it |
| --- | --- | --- | --- |
| `AXIS_LOW` | `1000` | bigger dead zone — needs a firmer push, less twitchy | more sensitive; too far and the robot creeps |
| `AXIS_HIGH` | `3000` | more sensitive on that side | needs a firmer push on that side |
| `SEND_INTERVAL_MS` | `100` | fewer messages, smoother radio link, laggier response | snappier, but more radio traffic. Keep `LINK_TIMEOUT_MS` at least 3× this |
| `RECONNECT_INTERVAL_MS` | `2000` | slower to recover after a dropout | faster recovery; too low spams Bluetooth discovery |
| `BT_SLAVE_NAME` | `"HC-05"` | — | must match `AT+NAME` on the module exactly |

**A rule that keeps you out of trouble:** `LINK_TIMEOUT_MS` must always be comfortably larger than
`SEND_INTERVAL_MS`. If you make the controller send every 500 ms while the robot's timeout is
500 ms, the robot will stutter and stop at random.

### Recovering if you break something

All three files are tracked in git. From the repository folder:

```bash
git diff                              # see exactly what you changed
git checkout -- arduino_nano_robot_RX_TX.ino    # restore one file
git checkout -- .                     # restore everything
```

---

## 12. Glossary

| Term | Meaning |
| --- | --- |
| **ADC** | Analog-to-Digital Converter. Turns a voltage into a number. The ESP32's is 12-bit, so 0–4095. |
| **baud** | Bits per second on a serial line. Both ends must agree. `115200` on the ESP32, `9600` on the HC-05. |
| **brown-out** | The supply voltage dips so low that a chip resets. Usually caused by a motor or servo drawing a current spike. |
| **buffer** | A small memory area where incoming bytes wait their turn to be read. |
| **char** | A single character, written `'F'`. Different from `String`. |
| **coast / brake** | Coast = motor terminals disconnected, wheel spins freely. Brake = terminals connected together, wheel resists turning. |
| **dead zone** | The band around a joystick's centre where movement is deliberately ignored. |
| **duty cycle** | The percentage of time a PWM signal is on. Sets motor speed. |
| **failsafe / watchdog** | A rule that takes over when normal operation stops. Here: no message for 500 ms → stop the motors. |
| **frame / packet** | One complete message. Here `FO\n`. |
| **GPIO** | General Purpose Input/Output. A pin you can program. `IO32` on the schematic = pin 32 in the code. |
| **H-bridge** | A circuit that can drive a motor in both directions. Two live inside the L298N. |
| **HC-05** | A cheap Bluetooth-to-serial module. Acts as the robot's radio receiver. |
| **HIGH / LOW** | The two digital states: roughly 5 V (or 3.3 V) and roughly 0 V. |
| **`INPUT_PULLUP`** | Pin mode that switches on an internal resistor so an unconnected button reads HIGH, and pressing it reads LOW. |
| **L298N** | The dual H-bridge motor driver board. Takes low-power signals from the Nano and switches high-current power to the motors. |
| **master / slave** | Who starts a Bluetooth connection. ESP32 = master, HC-05 = slave. |
| **`millis()`** | Milliseconds since power-on. Used for timing without freezing the program. |
| **PWM** | Pulse Width Modulation. Fast on/off switching used to vary effective power. Only on D3, D5, D6, D9, D10, D11 on a Nano. |
| **servo** | A motor that moves to a commanded angle (0–180°) and holds there. Used for the claw. |
| **sketch** | An Arduino program file (`.ino`). |
| **SPP** | Serial Port Profile. The Bluetooth flavour that pretends to be a serial cable. |
| **TT motor** | The small yellow-gearbox DC motor used on the chassis. |
| **untethered** | Running with no USB cable attached. Required, because USB and the HC-05 share the Nano's serial port. |

