#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Display hardware pins
// ---------------------------------------------------------------------------
#define LCD_BL_PIN   2   // Backlight PWM
#define LCD_RST_PIN 14   // Display hardware reset

// ---------------------------------------------------------------------------
// VESC UART
// ---------------------------------------------------------------------------
constexpr int      VESC_UART_RX_PIN  = 17;
constexpr int      VESC_UART_TX_PIN  = 18;
constexpr uint32_t VESC_UART_BAUD    = 115200;

// ---------------------------------------------------------------------------
// Throttle ADC
// ---------------------------------------------------------------------------
constexpr int      THROTTLE_ADC_PIN          = 16;
constexpr uint32_t THROTTLE_LOG_INTERVAL_MS  = 500;
constexpr uint16_t THROTTLE_IDLE_MV          = 850;   // ~0.85 V at idle
constexpr uint16_t THROTTLE_FULL_MV          = 2550;  // ~2.55 V at full
constexpr uint16_t THROTTLE_IDLE_DEADZONE_MV = 40;

// ---------------------------------------------------------------------------
// Main-loop tick intervals
// ---------------------------------------------------------------------------
constexpr uint32_t CONTROL_INTERVAL_MS   = 10;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 100;
constexpr uint32_t ALIVE_INTERVAL_MS     = 200;
constexpr uint32_t UI_INTERVAL_MS        = 100;

// ---------------------------------------------------------------------------
// Speed conversion
// Formula: (PI * wheel_diameter_m * 60) / (motor_pole_pairs * 1000)
// Hardware: 10" wheel (254 mm) + 30-pole motor (15 pairs) => ~0.003192
// Calibrate against a known speed source before trusting the display.
// ---------------------------------------------------------------------------
constexpr float kRpmToKmhFactor = 0.003192f;

// ---------------------------------------------------------------------------
// Control profile defaults
// ---------------------------------------------------------------------------
constexpr float CONTROL_INITIAL_DRIVE_CURRENT_A = 6.0f;
constexpr float CONTROL_INITIAL_BRAKE_CURRENT_A = 4.0f;
constexpr float CONTROL_SPEED_TAPER_START_KMH   = 16.0f;
constexpr float CONTROL_SPEED_LIMIT_KMH         = 60.0f;
constexpr float CONTROL_DRIVE_RAMP_RATE_APS     = 25.0f;
constexpr float CONTROL_BRAKE_RAMP_RATE_APS     = 35.0f;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
inline float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
