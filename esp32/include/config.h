#pragma once
// ==============================================================================
// config.h — Central configuration for the Scooter Control firmware
// Edit this file to match your hardware and riding preferences.
// ==============================================================================

// ------------------------------------------------------------------------------
// Waveshare ESP32-S3 1.28" Round Touch LCD — fixed on-board pin assignments
// (These match the PCB silkscreen; don't change unless you know what you're doing)
// ------------------------------------------------------------------------------
#define LCD_MOSI   11
#define LCD_SCLK   10
#define LCD_CS      9
#define LCD_DC      8
#define LCD_RST    12
#define LCD_BL     40   // Backlight — HIGH = on

#define TOUCH_SDA   6   // I2C SDA (shared with QMI8658 IMU)
#define TOUCH_SCL   7   // I2C SCL
#define TOUCH_INT   4   // CST816S interrupt (active LOW)
#define TOUCH_RST   5   // CST816S reset

// ------------------------------------------------------------------------------
// Input hardware — throttle, brake, buttons, indicators
// (Responsibility analysis: all inputs route through ESP32, not directly to
//  the VESC.  ESP32 reads ADC/GPIO, applies mode limits, then sends
//  COMM_SET_CURRENT / COMM_SET_CURRENT_BRAKE to the VESC over UART.
//  This gives software control of speed limiting, ride modes, throttle curves,
//  and graceful shutdown. See emulator/README.md for full wiring details.)
// ------------------------------------------------------------------------------

// Hall-effect throttle (0.8 V idle → 4.2 V full).
// Use a 3.9 kΩ / 10 kΩ voltage divider to stay within 3.3 V ADC range.
#define THROTTLE_PIN        1     // ADC1_CH0

// Brake lever — digital switch (active LOW, 10 kΩ pull-up) or Hall analog.
// Analog: read ADC and send proportional COMM_SET_CURRENT_BRAKE.
// Digital: send fixed regen current on press.
#define BRAKE_PIN           2     // GPIO (digital) or ADC1_CH1 (analog)
#define BRAKE_REGEN_AMPS    8.0f  // regen current for digital brake switch

// Turn signal buttons (active LOW, 10 kΩ pull-up)
#define BTN_TURN_LEFT       13
#define BTN_TURN_RIGHT      14

// Power button (active LOW, 10 kΩ pull-up)
// Short press: cycle display or reserved.  Long press (> 2 s): shutdown.
#define BTN_POWER           21
#define POWER_LONG_PRESS_MS 2000

// Turn signal LEDs (active HIGH)
#define LED_TURN_LEFT       15
#define LED_TURN_RIGHT      16

// Turn-signal blink period (ms)
#define BLINK_PERIOD_MS     500

// ------------------------------------------------------------------------------
// VESC UART
// ------------------------------------------------------------------------------
#define VESC_TX          17   // ESP32 TX → VESC RX
#define VESC_RX          18   // ESP32 RX ← VESC TX
#define VESC_BAUD        115200
#define VESC_UART_NUM    1    // HardwareSerial(1)

// How often the VESC is polled for telemetry (ms)
#define VESC_POLL_MS     100
// COMM_ALIVE heartbeat interval (ms) — must be < VESC timeout (500 ms)
#define VESC_ALIVE_MS    200
// Read timeout waiting for VESC response (ms)
#define VESC_TIMEOUT_MS  50

// ------------------------------------------------------------------------------
// WiFi Access Point
// ------------------------------------------------------------------------------
#define WIFI_SSID        "ScooterControl"
#define WIFI_PASSWORD    "scoot1234"
#define WIFI_CHANNEL     6
#define WIFI_MAX_CLIENTS 3

// Web server port
#define WEB_PORT         80
// WebSocket telemetry push interval (ms)
#define WS_PUSH_MS       200

// ------------------------------------------------------------------------------
// BLE
// ------------------------------------------------------------------------------
#define BLE_DEVICE_NAME  "ScooterCtrl"

// ------------------------------------------------------------------------------
// Display
// ------------------------------------------------------------------------------
#define DISPLAY_WIDTH    240
#define DISPLAY_HEIGHT   240
// LVGL draw buffer lines (larger = faster render, more RAM)
#define LV_BUF_LINES     60

// ------------------------------------------------------------------------------
// Scooter / Motor geometry  (Xiaomi Elite NE)
// Adjust MOTOR_POLE_PAIRS if your speed reads wrong: count motor magnets / 2
// ------------------------------------------------------------------------------
#define MOTOR_POLE_PAIRS    15      // 15 pairs = 30 poles (typical hub motor)
#define WHEEL_DIAMETER_MM   216     // 8.5" pneumatic ≈ 216 mm
#define WHEEL_CIRCUMFERENCE_M  (WHEEL_DIAMETER_MM * 3.14159f / 1000.0f)

// Speed helper: ERPM → km/h
// erpm / pole_pairs = mechanical RPM; × circumference × 60 / 1000 = km/h
#define ERPM_TO_KMH(erpm)  \
    ((float)(erpm) / MOTOR_POLE_PAIRS * WHEEL_CIRCUMFERENCE_M * 60.0f / 1000.0f)

// ------------------------------------------------------------------------------
// Battery (10S LiPo — 36 V nominal)
// ------------------------------------------------------------------------------
#define BATTERY_CELLS_S     10
#define CELL_VOLTAGE_MAX    4.20f
#define CELL_VOLTAGE_MIN    3.30f
#define BATTERY_VOLTAGE_MAX (BATTERY_CELLS_S * CELL_VOLTAGE_MAX)  // 42.0 V
#define BATTERY_VOLTAGE_MIN (BATTERY_CELLS_S * CELL_VOLTAGE_MIN)  // 33.0 V

// Convert measured voltage to battery percentage (0–100)
#define VOLTAGE_TO_PCT(v) \
    ((int)(((v) - BATTERY_VOLTAGE_MIN) / (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN) * 100.0f))

// ------------------------------------------------------------------------------
// Riding modes — current limits sent to VESC via UART
// ------------------------------------------------------------------------------
struct RideMode {
    const char* name;
    float motor_amps;    // COMM_SET_CURRENT limit
    float battery_amps;  // enforced by the VESC motor config absolute max
    int   max_erpm;
    uint32_t color;      // LVGL colour for UI indicator
};

// Defined in main.cpp
extern const RideMode RIDE_MODES[];
extern const int      RIDE_MODE_COUNT;
