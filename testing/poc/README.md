# POC — minimal "does it move + show speed?" tests

Two standalone files, both intentionally tiny and **independent of the rest of
the project's code**. Use them to validate the hardware before trusting the
bigger firmware.

Both POCs cap speed at **5 km/h** in software and assume a Xiaomi Elite NE hub
motor (15 pole pairs, 8.5" / 216 mm wheel → 0.679 m circumference).
Always lift the wheel off the ground for the very first test.

---

## HW responsibilities: what does what?

Understanding the division of responsibilities is essential for safe operation:

### VESC (motor controller — the MINI 6.7 hardware)
The VESC owns *all* power electronics and motor physics:
- FOC commutation and current loop (runs at ~30 kHz).
- Hardware current limits (`l_abs_current_max`, `lo_current_max`).
- Temperature protection — shuts down on MOSFET / motor overheat.
- Battery undervoltage protection.
- ERPM ceiling (`l_max_erpm`) — **set this to ≤2500 before ANY test**.
- **UART watchdog timeout**: if no UART command arrives within the configured
  timeout (default 500 ms), the VESC automatically sets current to 0 (freewheel).
  This is the hardware backstop if the ESP32 crashes.

### ESP32 / MCU
The MCU is the rider interface and telemetry display — it does NOT run the motor:
- Reads touch input or throttle signal.
- Sends VESC the appropriate *current* command: `COMM_SET_CURRENT`,
  `COMM_SET_CURRENT_BRAKE`, or `COMM_ALIVE`.
- Keeps the VESC alive (motor commands reset the watchdog, but explicit
  `COMM_ALIVE` is sent as a belt-and-suspenders measure every 200 ms).
- Reads `COMM_GET_VALUES` telemetry and enforces the software speed cap by
  stopping the current request when speed ≥ 5 km/h.
- Drives the display (speed readout, state banner).

### Why COMM_SET_CURRENT, not COMM_SET_RPM?

`COMM_SET_RPM` (ID=8) activates the VESC's internal PID speed controller.
That controller requires:
1. A completed motor-detection run (to identify R, L, flux linkage),
2. Properly tuned PID gains for the specific motor.

Without both, `COMM_SET_RPM` will typically jerk, oscillate, or fail to start
from zero (especially common with hub motors and Hall sensors).

`COMM_SET_CURRENT` (ID=6) directly commands the current loop, which **is** set
up by the motor-detection wizard.  It is reliable from the first run.

Protocol reference (from `vedderb/bldc` `comm/commands.c`):
```
case COMM_SET_CURRENT:
    mc_interface_set_current((float)buffer_get_int32(data, &ind) / 1000.0)
```
→ Send `amps × 1000` as a big-endian `int32`.  Positive = drive, 0 = coast.

```
case COMM_SET_CURRENT_BRAKE:
    mc_interface_set_brake_current((float)buffer_get_int32(data, &ind) / 1000.0)
```
→ Same encoding; positive values only; maps to regenerative braking.

### COMM_GET_VALUES RPM field (verified offset)

The response payload byte layout (full mask, no `SELECTIVE` prefix):
```
[0]     cmd echo = 0x04
[1..2]  temp_fet      float16 ÷ 10
[3..4]  temp_motor    float16 ÷ 10
[5..8]  avg_motor_current  float32 ÷ 100
[9..12] avg_input_current  float32 ÷ 100
[13..16] avg_id  float32 ÷ 100
[17..20] avg_iq  float32 ÷ 100
[21..22] duty    float16 ÷ 1000
[23..26] rpm     int32  (signed ERPM, scale 1.0)    ← speed field
[27..28] v_in    float16 ÷ 10
...
```
Source: `buffer_append_float32(send_buffer, mc_interface_get_rpm(), 1e0, &ind)`
where `buffer_append_float32` writes `(int32_t)(val × scale)` big-endian.

### CST816S touch controller gotchas

Two issues confirmed against `DS-CST816T-V2.2.pdf` and `koendv/cst816t`:

1. **Auto-sleep** (the big one): the chip enters deep sleep after ~2 s of
   inactivity and stops responding to I2C reads.  You **must** write `0xFF` to
   register `0xFE` (`REG_DIS_AUTOSLEEP`) during init or the touch will die
   seconds after boot.

2. **I2C reset timing**: the datasheet requires ≥ 50 ms after RST goes HIGH
   before the chip is ready.  The `koendv/cst816t` library uses 100 ms — use
   the same.

3. **I2C transaction pattern**: write register address → `endTransmission(true)`
   (STOP) → `requestFrom(…, true)`.  The official driver uses STOP+START, not
   repeated-start.

---

## `vesc_poc.py` — drive the VESC from a laptop (no ESP32)

Plug the VESC into your computer via USB-C (the MINI 6.7 enumerates as a
USB CDC serial port, no driver needed on Linux/macOS), then:

```bash
pip install pyserial
python3 vesc_poc.py            # auto-detect port
python3 vesc_poc.py /dev/ttyACM0
python3 vesc_poc.py COM5       # Windows
```

Keys (no Enter needed): `g` = gas (3 A), `b` = brake (2 A regen),
`s` = stop/coast (0 A), `q` = quit.
Live speed prints once per second.  The script cuts gas automatically if
speed ≥ 5 km/h.

## `mcu_poc.ino` — drive the VESC from the ESP32-S3 with on-screen speed

Copy `mcu_poc.ino` into its own PlatformIO or Arduino project (do **not** build
it as part of the existing `esp32/` project — they would clash).

Required libraries:

- `bodmer/TFT_eSPI` configured for GC9A01 (use the same `build_flags` block
  from [esp32/platformio.ini](../esp32/platformio.ini))
- `Wire` (built-in)

Controls (touch the round screen):

- **Upper half** → GAS (3 A motor current, cuts out at ≥ 5 km/h)
- **Lower half** → BRAKE (2 A regen)
- **No touch** → COAST (0 A — motor freewheels)

State banner (green/red/yellow/orange) and speed in km/h are shown live.

---

## Before either POC

1. Run VESC Tool motor-detection wizard (see [vesc/README.md](../vesc/README.md)).
2. In VESC Tool App Settings → UART: enable UART app, set timeout to 500 ms.
3. In VESC Tool Motor Settings → General: set `l_max_erpm` to ≤ 2500 as a
   hardware backstop.  The scripts enforce their own software limit on top.
4. Check motor direction: at very low throttle the wheel should spin
   forward.  If it goes backward, swap any two motor phase wires.
