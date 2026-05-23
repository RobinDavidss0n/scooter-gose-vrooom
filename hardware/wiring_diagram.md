# Wiring Diagram

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
