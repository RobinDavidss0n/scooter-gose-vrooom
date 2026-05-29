# Hardware Emulator

Emulates all physical hardware so the firmware can be developed and tested
without any real hardware connected.

---

## Quick Start

```bash
# From the repo root
python3 emulator/vesc_emulator.py

# Headless (CI / no terminal)
python3 emulator/vesc_emulator.py --no-tui

# Start with 50% battery
python3 emulator/vesc_emulator.py --soc 0.5
```

The emulator creates a virtual serial port (PTY) and prints the slave path,
for example `/dev/pts/4`. It also creates a stable symlink at `/tmp/vesc_emu`.

Point your ESP32 firmware (or any VESC UART client) at that path:

```
screen /tmp/vesc_emu 115200
# or
minicom -D /tmp/vesc_emu -b 115200
```

To bridge the virtual port to a real USB-UART adapter (e.g. when the actual
ESP32 is connected but the VESC is not):

```bash
socat /tmp/vesc_emu,rawer /dev/ttyUSB0,b115200,rawer
```

---

## TUI Controls

| Key | Action |
|-----|--------|
| `t` / `T` | Throttle +5 % / −5 % |
| `↑` / `↓` | Throttle +10 % / −10 % |
| `b` / `B` | Brake +5 % / −5 % |
| `SPACE` | Full brake (brake = 100 %, throttle = 0 %) |
| `l` | Toggle left turn signal |
| `r` | Toggle right turn signal |
| `p` | Toggle power on/off |
| `f` | Inject a random fault code |
| `0` | Clear injected fault |
| `q` | Quit |

---

## What Is Emulated

### VESC UART Protocol (115200 baud, VESC packet framing)

All packets use VESC's standard framing:

```
Short frame (payload ≤ 255 B):
  [0x02][LEN][...PAYLOAD...][CRC_H][CRC_L][0x03]

Long frame (payload > 255 B):
  [0x03][LEN_H][LEN_L][...PAYLOAD...][CRC_H][CRC_L][0x03]

CRC: CRC16-CCITT (poly 0x1021, init 0x0000)
```

| Command | ID | Direction | Handled |
|---|---|---|---|
| `COMM_FW_VERSION` | 0 | ESP32 → VESC (req) | ✓ Returns FW 5.3, HW "MINI 6.7" |
| `COMM_GET_VALUES` | 4 | ESP32 → VESC (req) | ✓ Full 59-byte telemetry response |
| `COMM_SET_DUTY` | 5 | ESP32 → VESC | ✓ Updates physics (no ACK) |
| `COMM_SET_CURRENT` | 6 | ESP32 → VESC | ✓ Updates physics (no ACK) |
| `COMM_SET_CURRENT_BRAKE` | 7 | ESP32 → VESC | ✓ Updates physics (no ACK) |
| `COMM_SET_RPM` | 8 | ESP32 → VESC | ✓ Speed PI controller (no ACK) |
| `COMM_SET_HANDBRAKE` | 10 | ESP32 → VESC | ✓ Same as SET_CURRENT_BRAKE |
| `COMM_GET_DECODED_PPM` | 18 | ESP32 → VESC (req) | ✓ Returns neutral 1.5 ms |
| `COMM_GET_DECODED_ADC` | 19 | ESP32 → VESC (req) | ✓ Returns 0 V |
| `COMM_ALIVE` | 30 | ESP32 → VESC | ✓ Resets UART timeout counter |
| `COMM_TERMINAL_CMD` | 16 | ESP32 → VESC | ✓ Echo reply via COMM_PRINT |
| `COMM_REBOOT` | 29 | ESP32 → VESC | ✓ Accepted, no-op |

### COMM_GET_VALUES Payload Layout

Exact byte-for-byte match with `bldc_interface.c` / `vesc_uart.cpp`:

```
Offset  Bytes  Scale    Field
  0      1     —        COMM_GET_VALUES (0x04)
  1      2     ×10      temp_mos         (°C, int16 big-endian)
  3      2     ×10      temp_motor       (°C)
  5      4     ×100     avg_motor_current (A, int32 big-endian)
  9      4     ×100     avg_input_current (A)
 13      4     ×100     avg_id           (d-axis A)
 17      4     ×100     avg_iq           (q-axis A)
 21      2     ×1000    duty_cycle       (0.0–1.0)
 23      4     ×1       rpm              (ERPM, int32)
 27      2     ×10      v_in             (V)
 29      4     ×10000   amp_hours        (Ah)
 33      4     ×10000   amp_hours_charged
 37      4     ×10000   watt_hours       (Wh)
 41      4     ×10000   watt_hours_charged
 45      4     —        tachometer       (counts, int32)
 49      4     —        tachometer_abs
 53      1     —        fault_code       (uint8)
 54      4     ×1e6     pid_pos          (deg, float32)
 58      1     —        vesc_id          (uint8)
```

### Physics Simulation (Xiaomi Elite NE)

- **Motor**: 3-phase hub, 15 pole pairs, 8.5″ wheel (216 mm diameter)
- **Vehicle**: ~90 kg (scooter + rider), empirical torque constant 0.088 (m/s²)/A
- **VESC UART timeout**: if no `COMM_ALIVE` for 500 ms, motor current is cut
  (exactly like the real VESC — tests the firmware's heartbeat task)
- **Thermal model**: FET and motor temperature rise with I²·R losses, cool
  with ambient air — triggers `OVER_TEMP_FET` / `OVER_TEMP_MOTOR` faults
- **Battery**: 10S LiPo, OCV curve, voltage sag under load, regen charging

### Simulated Input Hardware

These represent the physical sensors wired into the ESP32:

| Signal | Real hardware | TUI key | ESP32 pin* |
|--------|--------------|---------|-----------|
| Throttle (0–100 %) | Hall sensor, 0.8–4.2 V | `t`/`T`/`↑`/`↓` | GPIO 1 (ADC) |
| Brake (0–100 %) | Lever switch or Hall | `b`/`B`/`SPACE` | GPIO 2 (ADC/GPIO) |
| Turn signal left | Push button | `l` | GPIO 13 |
| Turn signal right | Push button | `r` | GPIO 14 |
| Power button | Push button | `p` | GPIO 21 |

\* Suggested pins — see hardware architecture section below.

---

## Hardware Architecture Analysis

> **TL;DR**: Throttle and brake go to the **ESP32**, not directly to the VESC.

### Option A — Throttle directly to VESC ADC

```
Hall throttle → VESC ADC pin → VESC drives motor
```

| | |
|---|---|
| Pro | One fewer MCU in the signal path; works if ESP32 crashes |
| Con | No software speed limiting; no mode-based current caps; no throttle curves; no data logging of throttle input |

### Option B — Throttle through ESP32 (recommended)

```
Hall throttle → ESP32 ADC → COMM_SET_CURRENT(mode.motor_amps × frac) → VESC
```

| | |
|---|---|
| Pro | Full software control: ride mode limits, expo curves, speed governor, logging |
| Pro | If ESP32 crashes, VESC UART timeout (500 ms) cuts motor → safe default |
| Pro | Single config file (`config.h`) controls all limits |
| Con | Tiny added latency (~10 ms) — negligible for a scooter |

**This project uses Option B.** The VESC is configured in **UART App** mode
(not ADC/PPM mode). The ESP32 reads all inputs and sends `COMM_SET_CURRENT`
on every control loop cycle.

### Responsibility breakdown

```
┌──────────────────────────────────────────────────────────────┐
│                        ESP32-S3                              │
│                                                              │
│  ADC ←── Hall throttle (0.8–4.2 V, divider to 3.3 V)        │
│  ADC ←── Brake lever analog  (optional, for proportional     │
│            regen)                                           │
│  GPIO ←── Brake switch (digital, active LOW, pull-up)        │
│  GPIO ←── Turn-L button  (active LOW, pull-up)               │
│  GPIO ←── Turn-R button  (active LOW, pull-up)               │
│  GPIO ←── Power button   (active LOW, pull-up)               │
│  GPIO ──► Turn-L LED                                         │
│  GPIO ──► Turn-R LED                                         │
│                                                              │
│  UART1 ──────────────────────────────────► VESC MINI 6.7     │
│          COMM_SET_CURRENT  (throttle)     drives hub motor   │
│          COMM_SET_CURRENT_BRAKE (brake)                      │
│          COMM_ALIVE        (every 200 ms)                    │
│          COMM_GET_VALUES   (every 100 ms) ◄── telemetry      │
│          COMM_FW_VERSION   (at boot)                         │
└──────────────────────────────────────────────────────────────┘
```

### Suggested GPIO additions to `config.h`

```cpp
// Throttle — Hall effect sensor (0.8–4.2 V)
// Connect through a 3.9 kΩ / 10 kΩ voltage divider to stay within 3.3 V
#define THROTTLE_PIN        1     // ADC1_CH0

// Brake — lever switch (active LOW) or analog Hall
#define BRAKE_PIN           2     // GPIO or ADC1_CH1

// Turn signals (active LOW, external 10 kΩ pull-up)
#define BTN_TURN_LEFT       13
#define BTN_TURN_RIGHT      14

// Power button (active LOW, external 10 kΩ pull-up, long-press = shutdown)
#define BTN_POWER           21

// Turn signal LEDs (active HIGH)
#define LED_TURN_LEFT       15
#define LED_TURN_RIGHT      16
```

### Throttle voltage divider

The Xiaomi Hall throttle outputs up to 4.2 V but the ESP32-S3 ADC is 3.3 V.
Use a resistor divider:

```
 5 V ─────┬──── Hall VCC
           │
  Throttle signal ──┬── 3.9 kΩ ──┬── ESP32 ADC pin
                    │             │
                    │           10 kΩ
                    │             │
                   GND           GND
```

Scale in firmware: `throttle_fraction = (adc_v * (3.9+10)/10 - 0.8) / (4.2 - 0.8)`

### Power-off sequence

```
User holds power button (> 2 s)
  → ESP32 sends COMM_SET_CURRENT(0.0)
  → ESP32 sends COMM_ALIVE to reset VESC timeout
  → ESP32 calls esp_deep_sleep_start()
  → VESC detects UART timeout after 500 ms → motor stays off
```

---

## Using with an ESP32 connected over USB

If your ESP32 hardware is available but the VESC is not:

1. Start the emulator: `python3 emulator/vesc_emulator.py`
2. Note the slave path, e.g. `/dev/pts/4`
3. Bridge the ESP32's VESC UART to the emulator:

```bash
# ESP32 USB-CDC appears as /dev/ttyACM0
# VESC pins (17/18) go through a USB-UART adapter at /dev/ttyUSB0
socat /dev/ttyUSB0,b115200,rawer /tmp/vesc_emu,rawer
```

4. Flash and run the firmware normally — it will talk to the emulator.

---

## Running without any hardware (pure simulation)

When no ESP32 is sending `COMM_SET_CURRENT`, the emulator falls back to its
**standalone simulation mode**: the TUI throttle/brake sliders drive the
physics engine directly (mimicking what the ESP32 firmware would compute from
the ADC readings in SPORT mode at 20 A peak current).

This is useful for:
- Verifying the `COMM_GET_VALUES` byte encoding is correct
- Testing display/web/BLE code against realistic telemetry values
- Smoke-testing fault injection paths (`[f]` key)
- Validating the UART timeout / heartbeat logic

---

## Requirements

Python 3.8+ standard library only — no extra packages needed.

```bash
python3 --version   # 3.8 or newer
python3 emulator/vesc_emulator.py --help
```
