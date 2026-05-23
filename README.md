# Scooter Gose Vrooom 🛴⚡🪿

Custom controller swap for the **Xiaomi Elite NE** — replacing the stock MCU + ESC with a full open-source VESC-based stack.

## Hardware

| Component | Part |
|---|---|
| ESC | Makerbase VESC MINI 6.7 (VESC 6.6-based, 13S) |
| DC-DC | XL7015 / LM2596 — 5V–80V → adjustable 5V–20V |
| Controller | Waveshare ESP32-S3 1.28″ Touchscreen (GC9A01, 240×240, CST816S touch) |
| Motor | Xiaomi Elite NE hub motor (3-phase BLDC + Hall sensors) |
| Battery | Stock Xiaomi Elite NE battery (36V / 10S) |

## Architecture

```
[Battery 36V]
     │
     ├──→ [VESC MINI 6.7] ──→ [Hub Motor]
     │        │
     │        │ UART (115200)
     │        ↓
     │    [ESP32-S3] ←── [Hall throttle]
     │        │
     │        ├──→ [Round LCD GC9A01] — local dashboard
     │        ├──→ WiFi AP 192.168.4.1 — web dashboard (phone)
     │        └──→ BLE GATT — telemetry + control via BLE apps
     │
     └──→ [XL7015 DC-DC] ──→ 5V ──→ [ESP32-S3 5V pin]
```

## Features

- **Round touchscreen dashboard** — speed arc, battery ring, temp, current, mode selector
- **WiFi Access Point** — connect phone to `ScooterControl` → open `192.168.4.1`
- **WebSocket telemetry** — live data pushed to browser every 200 ms
- **REST API** — `/api/status`, `/api/set` for scripting / app integration
- **BLE GATT server** — Nordic UART Service + custom telemetry characteristics
- **VESC config templates** — ready-to-import XML for VESC Tool 6.x
- **FreeRTOS** — display on Core 1, VESC polling on Core 0, async web on Core 0

## Quick Start

### 1. Flash the ESP32-S3

```bash
cd esp32
pio run --target uploadfs   # upload web files to SPIFFS
pio run --target upload     # flash firmware
pio device monitor          # open serial monitor
```

### 2. Configure the VESC

1. Install [VESC Tool](https://vesc-project.com/vesc_tool)
2. Connect VESC MINI 6.7 via USB
3. **Motor Wizard** → run detection (measures R, L, λ automatically)
4. Import `vesc/config/motor_config_xiaomi_elite_ne.xml` for limits/tuning
5. Import `vesc/config/app_config.xml` for UART app settings
6. Write configuration to VESC

### 3. Wire everything up

See `hardware/wiring_diagram.md` and `hardware/pinout.md`.

### 4. Connect your phone

- Join WiFi: **ScooterControl** / password: **scoot1234**
- Open browser: **http://192.168.4.1**

## Directory Structure

```
├── hardware/           Wiring diagrams, BOM, pinout reference
├── vesc/
│   ├── config/         VESC Tool import-ready XML configs
│   └── README.md       VESC setup walkthrough
└── esp32/
    ├── platformio.ini
    ├── include/        lv_conf.h, config.h
    ├── src/
    │   ├── main.cpp
    │   ├── vesc/       UART protocol driver
    │   ├── display/    GC9A01 + CST816S drivers, LVGL UI
    │   ├── wifi/       WiFi AP manager
    │   ├── web/        AsyncWebServer + WebSocket
    │   └── ble/        BLE GATT server
    └── data/           index.html, style.css, app.js (SPIFFS)
```

## VESC UART API (from ESP32)

The ESP32 talks to the VESC at **115200 baud** using the standard VESC packet protocol.

| Command | ID | Direction |
|---|---|---|
| `COMM_GET_VALUES` | 4 | ESP32 → VESC request; VESC replies with full telemetry |
| `COMM_SET_DUTY` | 5 | ESP32 → VESC; duty cycle –1.0 … 1.0 |
| `COMM_SET_CURRENT` | 6 | ESP32 → VESC; motor current A |
| `COMM_SET_CURRENT_BRAKE` | 7 | ESP32 → VESC; regen/brake current A |
| `COMM_SET_RPM` | 8 | ESP32 → VESC; target ERPM |
| `COMM_ALIVE` | 30 | ESP32 → VESC; heartbeat (keeps UART timeout from triggering) |

## REST API (via phone browser)

```
GET  /api/status          → JSON snapshot of all VESC values
GET  /api/mode?m=eco      → Set riding mode (eco / sport / turbo)
POST /api/set             → JSON body { "max_current": 15.0 }
WS   /ws                  → WebSocket — pushed JSON every 200 ms
```

## Calibration Notes

- Run **VESC Tool motor detection wizard** before first ride — do NOT skip this
- Adjust `MOTOR_POLE_PAIRS` in `include/config.h` if speed reads wrong (Xiaomi hub ≈ 15 pairs)
- Adjust `WHEEL_DIAMETER_MM` (Elite NE uses 8.5″ ≈ 216 mm) if odometer is off
- Start with conservative limits (15A motor / 12A battery) and raise gradually

## Safety Warning

> This modification bypasses Xiaomi's original safety firmware. You are responsible for
> verifying all current limits, voltage limits, and thermal protection settings before riding.
> Always wear protective gear.
