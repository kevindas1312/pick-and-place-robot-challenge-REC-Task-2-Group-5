#!/usr/bin/env python3
"""
Corrected schematic - Pick & Place Robot (REC Task 2, Group 5) - REV 1.1

Regenerates the REV 1.0 drawing with the corrections found during review:
  * ENB moved off D10 (hijacked by the Servo library / Timer1) and off D7
    (not a PWM pin) onto D11 (Timer2A).
  * C1/C2 corrected from 0.1 F to 0.1 uF motor snubbers.
  * F1 3 A fuse added, C4 bulk cap on the +7.4 V motor rail added,
    C5 bulk cap at the servo shown as recommended.
  * Divider confirmed on the Nano-TX side only.
  * L298N jumper notes (ENA/ENB removed, 5 V fitted).
  * Joysticks on +3V3 (GPIO34/35 are NOT 5 V tolerant).

Run:  python3 make_schematic.py
Out:  schematic_rev1.1.png / schematic_rev1.1.pdf
"""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch

# ----------------------------------------------------------------- palette
INK      = "#141414"
MOD_FILL = "#eaf0fa"
MOD_EDGE = "#27486f"
WIRE     = "#141414"
PWR      = "#b42318"    # supply rails
SIG      = "#146c43"    # motor signals
FIX      = "#d9480f"    # the corrected ENB net
NC       = "#8b8b8b"    # not connected
NOTE_BG  = "#fff8e6"
NOTE_ED  = "#b8860b"

UM = "\u00b5"           # micro sign

fig, ax = plt.subplots(figsize=(18.0, 11.6))
ax.set_xlim(0, 180)
ax.set_ylim(0, 116)
ax.set_aspect("equal")
ax.axis("off")


# ------------------------------------------------------------------ helpers
def poly(pts, color=WIRE, lw=1.6, z=4, ls="-"):
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=color, lw=lw,
            ls=ls, solid_joinstyle="miter", solid_capstyle="butt", zorder=z)


def junction(x, y, color=WIRE, s=26):
    ax.scatter([x], [y], s=s, color=color, zorder=6, edgecolors="none")


def module(x0, y0, x1, y1, title, sub=None, fs=10.5, fill=MOD_FILL, edge=MOD_EDGE,
           tx=None, ty=None):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor=fill,
                           edgecolor=edge, lw=1.7, zorder=3))
    cx = (x0 + x1) / 2 if tx is None else tx
    cy = y1 - 2.4 if ty is None else ty
    ax.text(cx, cy, title, ha="center", va="center",
            fontsize=fs, fontweight="bold", color=edge, zorder=7)
    if sub:
        ax.text(cx, cy - 2.7, sub, ha="center", va="center",
                fontsize=7.2, color=edge, zorder=7)


def pin_r(x1, y, name, fs=7.4, color=INK):
    ax.text(x1 - 1.1, y, name, ha="right", va="center", fontsize=fs,
            color=color, zorder=7)
    return (x1, y)


def pin_l(x0, y, name, fs=7.4, color=INK):
    ax.text(x0 + 1.1, y, name, ha="left", va="center", fontsize=fs,
            color=color, zorder=7)
    return (x0, y)


def pin_t(x, y1, name, fs=7.4, color=INK):
    ax.text(x, y1 - 1.1, name, ha="center", va="top", fontsize=fs,
            color=color, zorder=7)
    return (x, y1)


def pin_b(x, y0, name, fs=7.4, color=INK):
    ax.text(x, y0 + 1.1, name, ha="center", va="bottom", fontsize=fs,
            color=color, zorder=7)
    return (x, y0)


def gnd(x, y, color=WIRE):
    for w, dy in ((2.4, 0.0), (1.5, -0.85), (0.7, -1.7)):
        ax.plot([x - w / 2, x + w / 2], [y + dy, y + dy], color=color, lw=1.5,
                zorder=5)


def res_v(x, y0, y1, label, ldx=1.5, fs=7.4):
    """vertical resistor from y0 (bottom) to y1 (top)"""
    lead = 1.6
    ya, yb = y0 + lead, y1 - lead
    poly([(x, y0), (x, ya)])
    poly([(x, yb), (x, y1)])
    n, amp = 6, 0.75
    ys = [ya + (yb - ya) * i / n for i in range(n + 1)]
    pts = [(x, ya)]
    for i in range(n):
        pts.append((x + (amp if i % 2 == 0 else -amp), (ys[i] + ys[i + 1]) / 2))
    pts.append((x, yb))
    poly(pts, lw=1.4)
    ax.text(x + ldx, (y0 + y1) / 2, label, ha="left", va="center",
            fontsize=fs, color=INK, zorder=7)


def cap_v(x, y0, y1, label, ldx=1.4, fs=7.4, color=WIRE):
    mid = (y0 + y1) / 2
    g = 0.45
    poly([(x, y0), (x, mid - g)], color=color)
    poly([(x, mid + g), (x, y1)], color=color)
    for yy in (mid - g, mid + g):
        ax.plot([x - 1.3, x + 1.3], [yy, yy], color=color, lw=2.1, zorder=5)
    if label:
        ax.text(x + ldx, mid, label, ha="left", va="center", fontsize=fs,
                color=INK, zorder=7)


def battery_v(x, y0, y1):
    mid = (y0 + y1) / 2
    poly([(x, y0), (x, mid - 1.0)])
    poly([(x, mid + 1.1), (x, y1)])
    ax.plot([x - 1.5, x + 1.5], [mid - 1.0, mid - 1.0], color=WIRE, lw=2.2, zorder=5)
    ax.plot([x - 0.75, x + 0.75], [mid + 0.15, mid + 0.15], color=WIRE, lw=2.2, zorder=5)
    ax.plot([x - 0.4, x + 0.4], [mid + 1.1, mid + 1.1], color=WIRE, lw=2.2, zorder=5)


def fuse_h(x0, x1, y, label):
    ax.add_patch(FancyBboxPatch((x0 + 0.7, y - 0.9), (x1 - x0) - 1.4, 1.8,
                                boxstyle="round,pad=0.16", facecolor="white",
                                edgecolor=WIRE, lw=1.5, zorder=5))
    poly([(x0, y), (x0 + 0.7, y)])
    poly([(x1 - 0.7, y), (x1, y)])
    poly([(x0 + 1.5, y), (x1 - 1.5, y)], lw=1.1)
    ax.text((x0 + x1) / 2, y + 2.1, label, ha="center", va="center",
            fontsize=7.4, color=INK)


def switch_h(x0, x1, y, label):
    poly([(x0, y), (x0 + 0.8, y)])
    poly([(x1 - 0.8, y), (x1, y)])
    ax.plot([x0 + 0.8, x1 - 1.0], [y, y + 1.6], color=WIRE, lw=1.6, zorder=5)
    ax.scatter([x0 + 0.8, x1 - 0.8], [y, y], s=20, color=WIRE, zorder=6)
    ax.text((x0 + x1) / 2, y + 2.3, label, ha="center", va="center",
            fontsize=7.4, color=INK)


def motor_box(x0, y0, x1, y1, label):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor="#f4f4f4",
                           edgecolor=INK, lw=1.5, zorder=3))
    ax.text((x0 + x1) / 2, (y0 + y1) / 2, label, ha="center", va="center",
            fontsize=7.6, color=INK, zorder=7)


def flag(x, y, text, color=PWR, side="right", dy=0.0):
    """net flag pointing away from the wire end at (x, y)"""
    dx = 1.0 if side == "right" else -1.0
    ax.text(x + dx, y + dy, text, ha=("left" if side == "right" else "right"),
            va="center", fontsize=7.5, color=color, fontweight="bold", zorder=7,
            bbox=dict(boxstyle="round,pad=0.2", fc="white", ec=color, lw=0.9))


def nc(x, y, horizontal=True):
    ax.text(x, y, "NC", ha="center", va="center", fontsize=6.6, color=NC,
            fontweight="bold", zorder=7)


def note_box(x, y, lines, w=60, title="REV 1.1 CORRECTIONS"):
    h = 3.0 + 2.6 * len(lines)
    ax.add_patch(Rectangle((x, y), w, h, facecolor=NOTE_BG, edgecolor=NOTE_ED,
                           lw=1.2, zorder=2))
    ax.text(x + 1.4, y + h - 1.7, title, ha="left", va="center", fontsize=8.8,
            fontweight="bold", color=NOTE_ED, zorder=7)
    for i, ln in enumerate(lines):
        ax.text(x + 1.4, y + h - 4.4 - 2.6 * i, ln, ha="left", va="center",
                fontsize=7.1, color=INK, zorder=7)


def panel_title(x, y, text, sub=None):
    ax.text(x, y, text, ha="center", va="center", fontsize=15, fontweight="bold",
            color=INK)
    if sub:
        ax.text(x, y - 3.0, sub, ha="center", va="center", fontsize=7.8,
                color="#555555")


# ===========================================================  ROBOT PANEL
panel_title(56, 112.5, "ROBOT", "2S LiPo 7.4 V  -  mast-mounted chassis  -  runs untethered")

# ---- power chain ------------------------------------------------------
battery_v(9, 98, 106)
ax.text(11.6, 102, "BT1\n2S LiPo\n7.4 V", ha="left", va="center", fontsize=7.2)
poly([(9, 98), (9, 95)])
gnd(9, 95)
poly([(9, 106), (13, 106)])
fuse_h(13, 19, 106, "F1  3 A")
poly([(19, 106), (22, 106)])
switch_h(22, 28, 106, "S1")
poly([(28, 106), (30, 106)])
ax.text(31.4, 104.3, "+7.4 V", color=PWR, fontweight="bold", fontsize=7.6)
poly([(30, 106), (62, 106), (62, 79), (64, 79)], color=PWR)
poly([(30, 106), (30, 95), (34, 95)], color=PWR)
junction(30, 106, color=PWR)

# C4 bulk cap on the motor rail
cap_v(56, 100, 106, "", color=PWR)
ax.text(57.4, 103, "C4\n470 " + UM + "F", ha="left", va="center", fontsize=6.8)
gnd(56, 100, color=PWR)

# ---- buck converter ---------------------------------------------------
module(34, 90, 54, 100, "U1 DC-DC BUCK")
pin_l(34, 95, "IN+"); pin_l(34, 92.5, "IN-")
pin_r(54, 95, "OUT+"); pin_r(54, 92.5, "OUT-")
ax.text(44, 90.9, "7.4 V to 5 V", ha="center", va="center", fontsize=6.8, color=MOD_EDGE)
poly([(34, 92.5), (31.5, 92.5), (31.5, 89.5)])
gnd(31.5, 89.5)
poly([(54, 95), (57, 95), (57, 87)], color=PWR)
poly([(54, 92.5), (60, 92.5), (60, 89.5)])
gnd(60, 89.5)

# ---- +5V rail ---------------------------------------------------------
poly([(8, 87), (57, 87)], color=PWR)
junction(57, 87, color=PWR)
ax.text(40, 88.6, "+5 V", color=PWR, fontweight="bold", fontsize=7.6, ha="center")
cap_v(8, 82, 87, "", color=PWR)
ax.text(9.4, 84.6, "C3 100 " + UM + "F", ha="left", va="center", fontsize=6.8)
gnd(8, 82, color=PWR)

# ---- Arduino Nano -----------------------------------------------------
module(12, 44, 42, 83, "ARDUINO NANO", tx=24)
poly([(37, 83), (37, 87)], color=PWR)
junction(37, 87, color=PWR)
pin_t(37, 83, "5V")
poly([(12, 79), (8, 79), (8, 76)])
pin_l(12, 79, "GND")
gnd(8, 76)

pin_r(42, 76, "D6")
pin_r(42, 73, "D5")
pin_r(42, 70, "D4")
pin_r(42, 67, "D3")
pin_r(42, 64, "D2")
ax.text(40.9, 55, "D11", ha="right", va="center", fontsize=7.4,
        color=FIX, fontweight="bold", zorder=7)
ax.text(17.5, 45.2, "D1/TX", ha="right", va="bottom", fontsize=7.4, zorder=7)
ax.text(28.5, 45.2, "D0/RX", ha="left", va="bottom", fontsize=7.4, zorder=7)
ax.text(37.0, 45.2, "D9", ha="left", va="bottom", fontsize=7.4, zorder=7)

# ---- L298N ------------------------------------------------------------
module(64, 51, 88, 86, "MD1 L298N")
pin_l(64, 79, "12V")
pin_l(64, 76, "GND")
pin_l(64, 69, "ENA")
pin_l(64, 66, "IN1")
pin_l(64, 63, "IN2")
pin_l(64, 60, "IN3")
pin_l(64, 57, "IN4")
ax.text(65.1, 52, "ENB", ha="left", va="center", fontsize=7.4,
        color=FIX, fontweight="bold", zorder=7)
pin_r(88, 75, "OUT1")
pin_r(88, 68, "OUT2")
pin_r(88, 61, "OUT3")
pin_r(88, 54, "OUT4")

poly([(64, 76), (60, 76), (60, 73)])
gnd(60, 73)

# ---- Nano -> L298N signal wires ---------------------------------------
for (y0, xb, y1) in ((76, 46, 69), (73, 48, 66), (70, 50, 63),
                     (67, 52, 60), (64, 54, 57)):
    poly([(42, y0), (xb, y0), (xb, y1), (64, y1)], color=SIG)
poly([(42, 55), (58, 55), (58, 52), (64, 52)], color=FIX, lw=2.2)

# ---- motors + snubbers ------------------------------------------------
poly([(88, 75), (94, 75)]); poly([(88, 68), (94, 68)])
motor_box(94, 66, 107, 77, "M1\nTT")
cap_v(90.5, 68, 75, "")
ax.text(92.9, 71.5, "C1  0.1 " + UM + "F", rotation=90, ha="center", va="center",
        fontsize=6.0)
junction(90.5, 75); junction(90.5, 68)

poly([(88, 61), (94, 61)]); poly([(88, 54), (94, 54)])
motor_box(94, 52, 107, 63, "M2\nTT")
cap_v(90.5, 54, 61, "")
ax.text(92.9, 57.5, "C2  0.1 " + UM + "F", rotation=90, ha="center", va="center",
        fontsize=6.0)
junction(90.5, 61); junction(90.5, 54)

# ---- servo ------------------------------------------------------------
poly([(36, 44), (36, 32.5), (60, 32.5)], color=SIG)
module(60, 25, 78, 37, "SG1 MG90S")
pin_l(60, 32.5, "SIG", color=SIG)
pin_l(60, 30.5, "V+")
pin_l(60, 28.5, "GND")
poly([(60, 30.5), (56, 30.5)])
flag(56, 30.5, "+5 V", side="left")
poly([(60, 28.5), (56, 28.5), (56, 26)])
gnd(56, 26)

# C5 recommended
cap_v(84, 29, 35, "", color="#888888")
ax.text(85.4, 33.4, "C5", ha="left", va="center", fontsize=6.4, color="#666666")
ax.text(85.4, 31.6, "470 " + UM + "F", ha="left", va="center", fontsize=6.4,
        color="#666666")
ax.text(85.4, 29.8, "recommended", ha="left", va="center", fontsize=6.0,
        color="#666666", style="italic")
poly([(84, 35), (84, 37)], color="#888888")
flag(84, 37, "+5 V", side="right")
gnd(84, 29, color="#888888")


# ---- HC-05 + level divider -------------------------------------------
module(12, 4, 34, 16, "HC-05 BT", tx=23, ty=7)
pin_t(18, 16, "RX"); pin_t(28, 16, "TX"); pin_t(32, 16, "VCC")
ax.text(21.5, 11.6, "STATE / EN : NC", ha="center", va="center", fontsize=6.0,
        color=NC)
pin_l(12, 9, "GND")
poly([(12, 9), (9, 9), (9, 7)]); gnd(9, 7)
poly([(32, 16), (32, 20)]); flag(32, 20, "+5 V", side="right")

# Nano D1/TX -> R1 -> junction -> HC-05 RX   (divider on the TX side only)
poly([(18, 44), (18, 31)])
res_v(18, 25, 31, "")
ax.text(16.3, 28, "R1\n1 k", ha="right", va="center", fontsize=6.8)
junction(18, 25)
poly([(18, 25), (18, 16)])
poly([(18, 25), (21, 25)])
res_v(21, 20, 25, "")
ax.text(22.5, 22.5, "R2\n2 k", ha="left", va="center", fontsize=6.8)
gnd(21, 20)
# Nano D0/RX <- HC-05 TX   (direct, 3.3 V clears the ATmega's 3.0 V V_IH)
poly([(28, 44), (28, 16)])

# ---- correction callout ----------------------------------------------
ax.annotate("ENB moved to D11\n(D10 is owned by\nServo/Timer1)",
            xy=(54, 55), xytext=(44, 40.5), fontsize=6.8, color=FIX,
            ha="left", va="center", zorder=8,
            bbox=dict(boxstyle="round,pad=0.3", fc="white", ec=FIX, lw=1.1),
            arrowprops=dict(arrowstyle="->", color=FIX, lw=1.2))

ax.text(64, 48.9, "jumpers ENA / ENB  \u2192  REMOVED", fontsize=6.6,
        color=FIX, ha="left", va="center")
ax.text(64, 46.6, "5 V jumper  \u2192  FITTED (on-module regulator)", fontsize=6.6,
        color=INK, ha="left", va="center")

note_box(40, 2, [
    "1.  ENB : D10 (drawing) / D7 (code)  ->  D11   [Timer2A, PWM]",
    "     D10 is fast-PWM'd by the Servo library -> 0.5 % duty only.",
    "2.  C1, C2 : 0.1 F  ->  0.1 " + UM + "F (100 nF) motor snubbers.",
    "3.  F1 3 A fuse added; 2S LiPo low-voltage alarm recommended.",
    "4.  C4 470 " + UM + "F added on the +7.4 V rail, C3 100 " + UM + "F on +5 V.",
    "5.  R1/R2 divider on the Nano-TX side; HC-05 TX (3.3 V) -> Nano RX direct.",
    "6.  Unplug the HC-05 from D0/D1 to upload - it shares the CH340.",
], w=64)


# ======================================================  CONTROLLER PANEL
poly([(111, 4), (111, 112)], color="#aaaaaa", lw=1.0, ls=(0, (6, 4)))
panel_title(147, 112, "CONTROLLER", "hand-held remote  -  ESP32 on its own USB supply")

# ---- joysticks --------------------------------------------------------
module(114, 78, 134, 94, "U3 HW-504", "J1  left stick")
pin_r(134, 91, "Vcc"); pin_r(134, 88, "VRx")
pin_r(134, 85, "VRy"); pin_r(134, 82, "SW")
pin_b(124, 78, "GND")
poly([(124, 78), (124, 74)]); gnd(124, 74)

module(114, 50, 134, 66, "U4 HW-504", "J2  right stick")
pin_r(134, 63, "Vcc"); pin_r(134, 60, "VRx")
pin_r(134, 57, "VRy"); pin_r(134, 54, "SW")
pin_b(124, 50, "GND")
poly([(124, 50), (124, 46)]); gnd(124, 46)

# Vcc on +3V3 (GPIO34/35 are not 5 V tolerant)
poly([(134, 91), (137, 91)]); flag(137, 91, "+3V3", color="#7d3c98", side="right")
poly([(134, 63), (137, 63)]); flag(137, 63, "+3V3", color="#7d3c98", side="right")

# ---- ESP32 ------------------------------------------------------------
module(140, 46, 170, 104, "U5 ESP32-DEVKITC-32D", "Bluetooth SPP master",
       tx=155, ty=99.5, fs=9.2)
pin_t(167, 104, "3V3")
poly([(167, 104), (167, 108)])
flag(167, 108, "+3V3", color="#7d3c98", side="right")
pin_b(155, 46, "GND")
poly([(155, 46), (155, 42)]); gnd(155, 42)

pin_l(140, 88, "IO32"); pin_l(140, 85, "IO33"); pin_l(140, 82, "IO25")
pin_l(140, 60, "IO34"); pin_l(140, 57, "IO35"); pin_l(140, 54, "IO26")

poly([(134, 88), (140, 88)], color=SIG)
poly([(134, 85), (140, 85)], color=SIG)
poly([(134, 82), (140, 82)], color=SIG)
poly([(134, 60), (140, 60)], color=SIG)
poly([(134, 57), (140, 57)], color=SIG)
poly([(134, 54), (140, 54)], color=SIG)

ax.text(155, 66, "powered from\nUSB 5 V", ha="center", va="center",
        fontsize=6.8, color=NC, style="italic")
ax.text(155, 76, "master: connects to\n\"HC-05\" on boot", ha="center",
        va="center", fontsize=6.8, color=MOD_EDGE)

# ---- controller notes -------------------------------------------------
note_box(114, 20, [
    "HW-504 Vcc on +3V3 - GPIO34/35 are input-only and NOT 5 V tolerant.",
    "SW pins need no resistors: firmware uses INPUT_PULLUP.",
    "No 5 V rail on the remote - the ESP32 runs from its own USB supply.",
], w=62, title="CONTROLLER NOTES")

# ---- title block ------------------------------------------------------
ax.add_patch(Rectangle((114, 2), 62, 14, facecolor="white", edgecolor=INK, lw=1.4))
poly([(114, 10), (176, 10)], color=INK, lw=1.0)
poly([(150, 2), (150, 10)], color=INK, lw=1.0)
ax.text(145, 12.4, "Pick & Place Robot  -  Group 5", ha="center", va="center",
        fontsize=8.6, fontweight="bold")
ax.text(116, 6, "REV  1.1", ha="left", va="center", fontsize=7.4, fontweight="bold")
ax.text(116, 3.4, "corrected", ha="left", va="center", fontsize=6.6)
ax.text(152, 6, "SHEET  1/1", ha="left", va="center", fontsize=7.4)
ax.text(152, 3.4, "REC Task 2", ha="left", va="center", fontsize=6.6)

fig.savefig("schematic_rev1.1.png", dpi=220, bbox_inches="tight",
            facecolor="white")
fig.savefig("schematic_rev1.1.pdf", bbox_inches="tight", facecolor="white")
fig.savefig("schematic_rev1.1.svg", bbox_inches="tight", facecolor="white")
print("written: schematic_rev1.1.png / .pdf / .svg")

