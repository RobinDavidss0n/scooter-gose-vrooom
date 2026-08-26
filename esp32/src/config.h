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
constexpr uint16_t THROTTLE_IDLE_MV          = 850;  // The actual throttle hardware gives around 0.8v at idle
constexpr uint16_t THROTTLE_FULL_MV          = 2550; // The actual throttle hardware gives around 2.8v at full
constexpr uint16_t THROTTLE_IDLE_DEADZONE_MV = 40;

// ---------------------------------------------------------------------------
// Main-loop tick intervals
// ---------------------------------------------------------------------------
constexpr uint32_t CONTROL_INTERVAL_MS   = 10;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 25;
constexpr uint32_t ALIVE_INTERVAL_MS     = 500;
constexpr uint32_t UI_INTERVAL_MS        = 100;

// ---------------------------------------------------------------------------
// VESC link health thresholds
// ---------------------------------------------------------------------------
// Declare offline if no valid RX byte received within this window.
constexpr uint32_t VESC_CONNECTED_TIMEOUT_MS    = 1000;
// Declare telemetry stale if no successful GET_VALUES decode within this window.
// 3 missed frames at 40 Hz = 75 ms.
constexpr uint32_t VESC_TELEMETRY_MAX_AGE_MS    = 75;
// Show comms warning if any frame error occurred within this window.
constexpr uint32_t VESC_RX_ERROR_WINDOW_MS      = 1000;

// ---------------------------------------------------------------------------
// Speed conversion
// Formula: (PI * wheel_diameter_m * 60) / (motor_pole_pairs * 1000)
// Hardware: 10" wheel (254 mm) + 30-pole motor (15 pairs) => ~0.003192
// Calibrate against a known speed source before trusting the display.
// ---------------------------------------------------------------------------
constexpr float kRpmToKmhFactor = 0.003192f;

// ---------------------------------------------------------------------------
// Control profile defaults
constexpr float CONTROL_INITIAL_DRIVE_CURRENT_A = 6.0f;
constexpr float CONTROL_INITIAL_BRAKE_CURRENT_A = 4.0f;
constexpr float CONTROL_SPEED_LIMIT_KMH         = 30.0f;
// Taper begins at this fraction of the limit (60 %), giving a wide band to settle in.
// Adjust kTaperRatio if you want a shorter or longer ramp.
constexpr float CONTROL_SPEED_TAPER_RATIO       = 0.60f;
constexpr float CONTROL_SPEED_TAPER_START_KMH   = CONTROL_SPEED_LIMIT_KMH * CONTROL_SPEED_TAPER_RATIO;
constexpr float CONTROL_DRIVE_RAMP_RATE_APS     = 25.0f; //TODO control what this value should be
constexpr float CONTROL_BRAKE_RAMP_RATE_APS     = 35.0f; //TODO control what this value should be
// Minimum speed-scale floor: current never drops fully to zero at the limit.
// Enough to hold speed against real friction without bang-bang oscillation.
constexpr float CONTROL_SPEED_SCALE_FLOOR       = 0.08f;

// ---------------------------------------------------------------------------
// Lights
// Rear light stays on a native MCU PWM pin (needs real brightness levels for
// idle vs. brake). Front light is plain on/off and lives on the MCP23017
// expander instead (see Turn signals section below) — that freed up GPIO15
// on the MCU so I2C SCL could move off the GPIO0/BOOT strapping pin.
// Pins are placeholders — confirm against wiring/diagram.mmd before flashing.
// ---------------------------------------------------------------------------
constexpr int      REAR_LIGHT_PIN            = 21;
constexpr uint8_t  REAR_LIGHT_PWM_CHANNEL    = 1;
constexpr uint32_t LIGHT_PWM_FREQ_HZ         = 5000;
constexpr uint8_t  LIGHT_PWM_RESOLUTION_BITS = 8;

// Default brightness (0-255). Not tied to any control input yet — adjust
// here or via the console "light" command until dimming/day-night/touch
// control is decided.
constexpr uint8_t  REAR_LIGHT_DEFAULT_IDLE_BRIGHTNESS  = 40;
constexpr uint8_t  REAR_LIGHT_DEFAULT_BRAKE_BRIGHTNESS = 255;

// ---------------------------------------------------------------------------
// Turn signals (L/R buttons + blinker LEDs via MCP23017 I2C expander), plus
// the front light output (see Lights section above).
// See wiring/diagram.mmd. SCL rides GPIO15 — GPIO0/BOOT is deliberately left
// unconnected: it's a boot-strapping pin re-sampled on every chip reset (not
// just power-on), so a reset landing while I2C toggles it low could drop the
// board into UART download mode instead of resuming firmware.
// ---------------------------------------------------------------------------
constexpr int      MCP23017_SDA_PIN     = 33;
constexpr int      MCP23017_SCL_PIN     = 15;
constexpr uint8_t  MCP_BTN_LEFT_PIN     = 0;  // GPA0
constexpr uint8_t  MCP_BTN_RIGHT_PIN    = 1;  // GPA1
constexpr uint8_t  MCP_BLINK_LEFT_PIN   = 2;  // GPA2
constexpr uint8_t  MCP_BLINK_RIGHT_PIN  = 3;  // GPA3
constexpr uint8_t  MCP_FRONT_LIGHT_PIN  = 4;  // GPA4
constexpr uint32_t BUTTON_DEBOUNCE_MS   = 30;
constexpr uint32_t BLINKER_INTERVAL_MS  = 500;
// Only used while unrestrictedModeActive; otherwise the buttons toggle blinkers.
constexpr float    SPEED_LIMIT_STEP_RPM = 200.0f;

// ---------------------------------------------------------------------------
// Soft power switch. A handlebar button feeds the always-on 12V rail through a
// diode into a chassis-side latch transistor to bootstrap a relay (gates the
// VESC only — SD/12V stays always-on so there's power to bootstrap from) from
// fully off. MCP_PWR_HOLD_PIN is firmware's side of that latch: asserted high
// on boot to hold the relay closed past the initial button press, released to
// cut power. See wiring/diagram.mmd (PWR_RELAY/PWR_LATCH) and power.cpp.
//
// MCP_PWR_BTN_PIN reads the same button locally, via a 10kΩ/3.3kΩ divider on
// its switched-12V signal (~3V when pressed — safely under the MCP23017's
// logic levels, no second wire down to chassis needed). Unlike
// MCP_BTN_LEFT/RIGHT_PIN, this reads ACTIVE-HIGH (idle low via the divider's
// bottom resistor), not active-low via internal pull-up — see power.cpp.
// Held for POWER_BUTTON_SHUTDOWN_HOLD_MS while running, it triggers a
// graceful shutdown; a short tap does nothing, so a debounce glitch or brief
// bump can't cut power while riding.
// ---------------------------------------------------------------------------
constexpr uint8_t  MCP_PWR_HOLD_PIN     = 8;  // GPB0
constexpr uint8_t  MCP_PWR_BTN_PIN      = 9;  // GPB1
constexpr uint32_t POWER_BUTTON_SHUTDOWN_HOLD_MS = 2000;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
inline float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
