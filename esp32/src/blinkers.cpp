#include "blinkers.h"

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

bool unrestrictedModeActive = false;

namespace {

Adafruit_MCP23X17 mcp;

bool blinkerLeftOn  = false;
bool blinkerRightOn = false;
bool blinkVisible   = false;
uint32_t lastBlinkToggleMs = 0;

// Debounced, edge-triggered button state.
struct DebouncedButton {
    uint8_t  mcpPin;
    bool     stablePressed;
    uint32_t lastChangeMs;
};

DebouncedButton leftButton  = { MCP_BTN_LEFT_PIN,  false, 0 };
DebouncedButton rightButton = { MCP_BTN_RIGHT_PIN, false, 0 };

// Returns true exactly once, on the release->press edge, once debounced.
bool poll_button_pressed(DebouncedButton &button, uint32_t nowMs)
{
    const bool rawPressed = mcp.digitalRead(button.mcpPin) == LOW;  // active-low, internal pull-up
    if (rawPressed != button.stablePressed &&
        (nowMs - button.lastChangeMs) >= BUTTON_DEBOUNCE_MS) {
        button.stablePressed = rawPressed;
        button.lastChangeMs  = nowMs;
        return button.stablePressed;
    }
    return false;
}

}  // namespace

void blinkers_init()
{
    Wire.begin(MCP23017_SDA_PIN, MCP23017_SCL_PIN);
    mcp.begin_I2C();

    mcp.pinMode(MCP_BTN_LEFT_PIN, INPUT_PULLUP);
    mcp.pinMode(MCP_BTN_RIGHT_PIN, INPUT_PULLUP);
    mcp.pinMode(MCP_BLINK_LEFT_PIN, OUTPUT);
    mcp.pinMode(MCP_BLINK_RIGHT_PIN, OUTPUT);
    mcp.digitalWrite(MCP_BLINK_LEFT_PIN, LOW);
    mcp.digitalWrite(MCP_BLINK_RIGHT_PIN, LOW);
}

void blinkers_poll(uint32_t nowMs, scooter::CurrentControlProfile &profile)
{
    const bool leftPressed  = poll_button_pressed(leftButton, nowMs);
    const bool rightPressed = poll_button_pressed(rightButton, nowMs);

    if (unrestrictedModeActive) {
        if (leftPressed) {
            profile.speedLimitRpm = max(0.0f, profile.speedLimitRpm - SPEED_LIMIT_STEP_RPM);
            profile.speedTaperStartRpm = min(profile.speedTaperStartRpm, profile.speedLimitRpm);
        }
        if (rightPressed) {
            profile.speedLimitRpm += SPEED_LIMIT_STEP_RPM;
        }
    } else {
        // Pressing one side always cancels the other, same as a real turn signal.
        if (leftPressed) {
            blinkerLeftOn  = !blinkerLeftOn;
            blinkerRightOn = false;
        }
        if (rightPressed) {
            blinkerRightOn = !blinkerRightOn;
            blinkerLeftOn  = false;
        }
    }

    if (nowMs - lastBlinkToggleMs >= BLINKER_INTERVAL_MS) {
        lastBlinkToggleMs = nowMs;
        blinkVisible = !blinkVisible;
    }

    // In unrestricted mode the blinkers stay dark regardless of prior toggle state.
    const bool showLeft  = !unrestrictedModeActive && blinkerLeftOn  && blinkVisible;
    const bool showRight = !unrestrictedModeActive && blinkerRightOn && blinkVisible;
    mcp.digitalWrite(MCP_BLINK_LEFT_PIN,  showLeft  ? HIGH : LOW);
    mcp.digitalWrite(MCP_BLINK_RIGHT_PIN, showRight ? HIGH : LOW);
}
