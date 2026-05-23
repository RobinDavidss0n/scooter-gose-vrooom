# VESC Configuration

## Prerequisites

1. Download **VESC Tool** (free, open source): https://vesc-project.com/vesc_tool
2. Connect VESC MINI 6.7 to your PC via USB-C
3. Click **Connect** in VESC Tool

---

## Step 1 — Motor Detection (Required!)

> The XML configs in this directory contain *target limits* but **not** the motor-specific
> parameters (R, L, flux linkage). You must run motor detection first.

1. **Motor Settings → FOC → Motor Setup Wizard**
2. Select **Inrunner / Outrunner / Hub motor**
3. Run **Resistance & Inductance Detection** — motor must be free to spin
4. Run **Flux Linkage Detection** (spins motor briefly)
5. Run **Hall Sensor Detection** (if Hall sensors are wired)
6. Write configuration

---

## Step 2 — Import the Limit Config

1. **Motor Settings → Import XML** → select `motor_config_xiaomi_elite_ne.xml`
2. Review the values (especially current limits) — adjust if needed
3. **Write Motor Configuration**

## Step 3 — Import the App Config

1. **App Settings → Import XML** → select `app_config.xml`
2. **Write App Configuration**

---

## Step 4 — Test & Calibrate

1. **RT Data** tab → watch live values while manually spinning the wheel
2. Verify **ERPM** increases when wheel spins forward
3. Verify **Hall sensors** show clean transitions (no missed steps)
4. Throttle: if using ADC throttle, go to **App Settings → ADC** and calibrate min/max voltage

---

## Key Parameters Explained

### Motor Current vs Battery Current

- **Motor current** (`l_current_max`): peak current through motor windings. Can be higher than battery current briefly.
- **Battery current** (`l_in_current_max`): sustained current drawn from battery. Should match your fuse rating.

### ERPM vs Speed

```
speed_kmh = (erpm / pole_pairs) * (wheel_circumference_m * π) * 60 / 1000
```

For Xiaomi Elite NE (8.5″ wheel, 15 pole pairs):
- 30 km/h ≈ 11,000 ERPM
- 25 km/h ≈ 9,200 ERPM

Set `l_max_erpm` to match your desired speed limit.

### Riding Modes (set from ESP32)

The ESP32 switches modes by sending `COMM_SET_CURRENT` with different limits:

| Mode | Motor A | Battery A | Max ERPM |
|---|---|---|---|
| ECO | 8 A | 6 A | 7,000 |
| SPORT | 20 A | 15 A | 11,000 |
| TURBO | 30 A | 20 A | 13,000 |

> Note: Mode limits are enforced in software on the ESP32 side. VESC hardware limits are the absolute ceiling.

---

## VESC Tool App (Mobile)

VESC Tool is available as a mobile app (Android) — you can connect directly via BLE
if your VESC has a BLE module, or you can use the ESP32 as a bridge (future feature).
