# Wiring Diagram

---

## Opening the Scooter (Xiaomi Elite NE Physical Disassembly)

### Tools Required

| Tool | Use |
|---|---|
| T25 Torx driver | Top deck panel screws |
| T20 Torx driver | Smaller internal screws (ESC mount) |
| PH2 Phillips | Mud-flap and stem cover screws |
| Plastic pry tool / spudger | Lifting the deck panel from its seating lip |
| Heat gun or hair dryer | Warming the non-slip grip tape for clean removal |
| Multimeter | Verify battery voltage and identify wires |
| ESD wrist strap | Anti-static when handling the ESC PCB |
| Cable ties + zip tie gun | Cable management inside deck |
| Masking tape + marker | Labelling connectors before unplug |

### Step 1 — Power off and discharge

**Never work on the scooter with the battery live.**

1. Power the scooter fully off — hold the power button until the display shuts down.
2. If the scooter has a charging port accessible from the outside, wait 5 minutes after the last power-off for the ESC's bulk capacitors to discharge.
3. Do not proceed if the scooter was plugged in to charge — unplug and wait.

### Step 2 — Remove the non-slip grip tape (top of deck)

The access panel on the Elite NE is on the **top surface of the deck** (the platform you stand on), covered by the non-slip grip tape or a rubber mat:

1. Leave the scooter right-side up — no need to flip it.
2. Use a heat gun or hair dryer on low heat, moving slowly across the grip tape for ~30–60 seconds to soften the adhesive underneath.
3. Starting at one corner, work a plastic spudger or your fingernail under the grip tape edge and peel it back slowly and evenly. It should come off in one piece if heated enough.
4. Set the grip tape aside sticky-side-up on a clean surface — it can be re-applied when done.

> **Tip:** If the grip tape tears, replacement non-slip tape is ~$5 on AliExpress (search "Xiaomi scooter deck grip tape 8.5 inch"). Cut to size.

### Step 3 — Remove the top deck panel screws

With the grip tape removed, the deck panel is held by screws visible on the top surface:

1. Remove all screws — typically **6× T25 Torx** arranged along the perimeter of the panel. There may be additional screws near the stem base; inspect the full surface.
2. Two screws may be hidden under the rear mud-flap — remove it first (2× PH2 Phillips).
3. Note the screw positions; some may be different lengths. Keep them sorted by location.

### Step 4 — Lift the deck panel

1. Slide a plastic spudger along the panel seam to release any adhesive seal or weatherstrip.
2. Lift the panel from the **rear end** first — it has a locating lip at the front near the stem that slides out rather than lifts straight up.
3. The panel is attached to no wiring — it lifts completely free.

### Step 5 — Identify the stock controller and connectors

Inside the deck you will find:
- **Battery pack** — occupying the front ~60% of the deck cavity. The BMS is integrated inside the battery housing. **Do not puncture or probe the battery**.
- **Stock Xiaomi ESC/controller board** — a small PCB (usually green) mounted in the **rear section** of the deck with 3–4 M3 screws. The heatsink faces upward toward the removed panel.
- **Main battery harness** — thick red/black wires (~14 AWG) from the BMS to the ESC, typically via a 2-pin or 3-pin MX3.0 / SM connector. Nominal ~36 V (10S). **Use a multimeter to confirm 0 V before touching** — some BMS designs hold charge briefly after power-off.
- **Motor cable bundle** — exits through a rubber grommet in the rear wall of the deck, runs along the underside of the frame rail to the rear wheel. Contains 3× thick phase wires (yellow/blue/green) and a 5-wire Hall cable (red/black + 3 signal). Terminates at the ESC via a multi-pin connector.
- **Throttle cable** — thin 3-wire cable running from the stem/handlebar area down through the folding hinge into the deck. Terminates at the ESC via a small JST-XH or SM connector.
- **Brake/light cables** — additional low-current wires (can be left in place; reconnect later to ESP32 GPIOs if desired).

### Step 6 — Safely remove the stock ESC

> **Safety first**: confirm with a multimeter that the battery connector reads 0 V before touching anything. Some Xiaomi BMS designs have a soft-off mode that still holds the output rail live until a connector is physically removed.

1. **Label everything before unplugging.** Wrap a small strip of masking tape around each connector and write what it is (MOTOR, HALL, BATT, THROTTLE, BRAKE, LIGHT). A phone photo also helps.
2. Unplug connectors **one at a time**, squeezing the locking tab and pulling straight — do not twist or yank.
3. The battery main connector should be the **last** to unplug. Grip the connector body, not the wires. If it is stiff, use a small flathead to gently lever the locking tab.
4. Once all connectors are free, remove the 3–4 M3 screws holding the ESC to the deck floor.
5. Lift the ESC board out by the edges. Do not touch component surfaces — use an ESD strap or at minimum touch a grounded metal surface before handling.
6. Place the ESC in an anti-static bag or on a non-conductive surface. Do not stack anything on top of it.
7. **Inspect the main battery connector pins** for any signs of arcing or carbon deposits before connecting the VESC to the same harness.

### Step 7 — Mount the VESC MINI 6.7

The VESC MINI 6.7 is ~50 × 50 mm with a heatsink. It mounts in the same rear deck bay:
- Use M3 standoffs (4–6 mm) so the heatsink is not flush against the floor — it needs airflow.
- Orient the heatsink **upward** (same as the stock ESC) so it faces the deck panel, which acts as a heat spreader when closed.
- If space is tight, the board can mount vertically against the rear wall using a small L-bracket.
- Route the motor and Hall cables through the existing rear grommet hole.

### Step 8 — Route the DC-DC and ESP32

The XL7015 DC-DC and the Waveshare ESP32-S3 board fit in the remaining front space next to the battery housing:
- Secure with double-sided VHB tape or 3D-printed mounts to the deck ribs.
- Run the 5 V pair to the ESP32 `5V` / `GND` pins using 22 AWG twisted pair.
- Run the UART TX/RX pair (GPIO 17 / GPIO 18) from the ESP32 to the VESC — keep signal wires away from the phase wires to reduce interference.

### Step 9 — Seal and reassemble

1. Zip-tie all cables flat against the deck ribs — nothing should press against the battery casing.
2. Add the inline **30 A blade fuse** to the battery positive wire between the BMS connector and VESC B+.
3. **Bench-test before closing**: connect a 36 V / 2 A limited bench supply to the VESC B+ / B−, confirm VESC Tool connects over USB, motor detection runs without smoke.
4. Once verified: reseat the deck panel (front lip first, then press rear down), replace all screws, re-apply the grip tape.

---

## Overview

```
                         ┌─────────────────────────────────┐
  Xiaomi Battery 36V ───►│XT30+  VESC MINI 6.7        A B C│──► Motor phases
          │              │                                   │
          │              │    Hall A ──────────────────── HA │◄── Hall A (motor)
          │              │    Hall B ──────────────────── HB │◄── Hall B (motor)
          │              │    Hall C ──────────────────── HC │◄── Hall C (motor)
          │              │    Hall 5V ──────────────── H5V  │◄── Hall VCC (motor)
          │              │    Hall GND ─────────────── HGND │◄── Hall GND (motor)
          │              │                                   │
          │              │    ADC1 (3.3V signal) ──────────►│◄── Throttle signal
          │              │    3.3V/5V ─────────────────────►│──► Throttle VCC
          │              │    GND ─────────────────────────►│──► Throttle GND
          │              │                                   │
          │              │    UART TX (GPIO 17 on ESP32) ───►│ VESC RX
          │              │    UART RX (GPIO 18 on ESP32) ───◄│ VESC TX
          │              │    GND ────────────────────────►  │◄── Common GND
          └──────────────┘                                   │
          │                                                  └──────────────────
          │
          ▼
  ┌──────────────────┐
  │  XL7015 DC-DC    │   (pre-set to 5.0 V output!)
  │  IN+  IN-        │◄── Battery +36 V, GND
  │  OUT+ OUT-  5V ──┼──► ESP32-S3 5V pin
  └──────────────────┘
          │
          ▼
  ┌───────────────────────────────────────────────────────────────────────┐
  │  Waveshare ESP32-S3 1.28" Round Touch LCD                            │
  │                                                                      │
  │  5V ◄── XL7015 OUT+                                                  │
  │  GND ◄── XL7015 OUT- / VESC GND (common)                            │
  │                                                                      │
  │  GPIO17 TX ──► VESC RX                                               │
  │  GPIO18 RX ◄── VESC TX                                               │
  │                                                                      │
  │  (Display & touch internal on board — no external wiring needed)     │
  └───────────────────────────────────────────────────────────────────────┘
```

## Critical Safety Steps

1. **Set DC-DC output to 5.0 V** with a multimeter BEFORE connecting to ESP32.
2. **Fuse the main battery line** — 30 A blade fuse inline on the positive wire.
3. **Common GND** — VESC GND, DC-DC GND, and ESP32 GND must all be the same node.
4. **Insulate** all exposed solder joints with heat shrink.
5. **Capacitors** — solder 2× 1000 µF / 63 V caps across the VESC battery input pads to reduce voltage spikes.

## UART Wiring Detail

```
ESP32-S3            VESC MINI 6.7
──────────          ─────────────
GPIO 17 (TX) ──────► RX  (UART pin, check silkscreen)
GPIO 18 (RX) ◄────── TX  (UART pin)
GND          ──────── GND (shared)
```

> **Note:** VESC UART logic is 3.3 V — compatible directly with ESP32-S3.
> Do NOT connect 5 V logic to VESC UART pins.

## Throttle (Hall Effect, 3-wire)

```
Throttle wire        VESC ADC
─────────────        ─────────────────────
Red (VCC)   ──────── 3.3 V output pin
Black (GND) ──────── GND
Green (SIG) ──────── ADC1 (0–3.3 V analog)
```

Configure in VESC Tool: `App Settings → ADC → Voltage start = 0.85 V, Voltage end = 2.75 V`
(measure with multimeter, adjust to match your actual throttle range).

## Motor Phases + Hall Sensors

Standard colour conventions (may differ — verify with multimeter):

| Wire colour | Connection |
|---|---|
| Yellow | Phase A |
| Blue | Phase B |
| Green | Phase C |
| Red | Hall 5V |
| Black | Hall GND |
| Yellow/thin | Hall A |
| Blue/thin | Hall B |
| Green/thin | Hall C |

If the motor runs backwards or stutters, swap any **two** phase wires (not Hall).
If Hall detection fails, try swapping Hall A/B/C order until VESC Tool shows stable position.
