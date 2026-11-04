# Hardware - Schematic REV 1.1

Corrected version of the REV 1.0 EasyEDA drawing (dated 2026-11-01).

| File | Description |
| --- | --- |
| `make_schematic.py` | Generator script (matplotlib). Edit coordinates here, not the outputs. |
| `schematic_rev1.1.png` | Raster render, 3093 x 2009 px, 220 dpi |
| `schematic_rev1.1.pdf` | Vector render, for printing |
| `schematic_rev1.1.svg` | Vector render, for editing in Inkscape / Figma |

Regenerate after any change:

```bash
python3 make_schematic.py     # requires matplotlib
```

## What changed from REV 1.0

| # | REV 1.0 | REV 1.1 | Why |
| --- | --- | --- | --- |
| 1 | `ENB` on **D10** (firmware said D7) | `ENB` on **D11** | D7 is not a PWM pin; D10 is PWM but the Servo library owns Timer1, so `analogWrite(D10, v)` only sets `OCR1B` against `ICR1 = 39999` -> **0.5 % duty**, i.e. the right wheel never turns. D11 is Timer2A and free. |
| 2 | `C1`, `C2` = **0.1 F** | `C1`, `C2` = **0.1 uF** | 0.1 F is a supercapacitor across each motor: seconds of inrush, motors twitch on power-up. These are 100 nF snubbers. |
| 3 | no fuse | `F1` **3 A** | No short-circuit protection on a 2S LiPo. |
| 4 | no bulk cap on the motor rail | `C4` **470 uF** | Swallows motor inrush / direction-change current spikes. |
| 5 | - | `C5` 470 uF at the servo | Marked *recommended*: an MG90S stall can brown out the Nano on a shared 5 V rail. |
| 6 | divider ambiguous | `R1`/`R2` explicitly on the **Nano-TX** side only | Dividing the HC-05's 3.3 V TX down to 2.2 V would be below the ATmega's 3.0 V `V_IH`. |
| 7 | L298N jumpers unmarked | **ENA/ENB jumpers REMOVED**, 5 V jumper **FITTED** | With jumpers left in, `ENA`/`ENB` are shorted to the module's +5 V and a Nano pin driving LOW sinks current through a dead short. |

Unchanged and correct in REV 1.0: joystick `Vcc` on **+3V3** (GPIO34/35 are input-only and
not 5 V tolerant), `SW` pins on `INPUT_PULLUP` with no external resistors, separate 5 V buck,
common ground, and `ENA`/`IN1`..`IN4` on D6/D5/D4/D3/D2.

## Still not on the drawing

* **LiPo low-voltage alarm / cutoff** - a 2S pack below ~6.6 V is permanently damaged.
* The robot must run **untethered**: the HC-05 shares D0/D1 with the CH340, so plugging in
  USB holds the Nano's RX high and Bluetooth data cannot get through. Unplug the HC-05 to upload.
