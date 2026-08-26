#include "power.h"

#include "config.h"
#include "expander.h"

namespace {

uint32_t pressedSinceMs  = 0;
bool     wasPressed      = false;
bool     shutdownLatched = false;

}  // namespace

void power_init()
{
    mcp.pinMode(MCP_PWR_HOLD_PIN, OUTPUT);
    mcp.digitalWrite(MCP_PWR_HOLD_PIN, HIGH);

    // Active-high divider input, not a switch-to-ground — no internal pull-up.
    mcp.pinMode(MCP_PWR_BTN_PIN, INPUT);
}

void power_shutdown(scooter::CurrentControlInputs &inputs,
                     scooter::CurrentController &controller,
                     scooter::VescUart &vesc)
{
    if (shutdownLatched) {
        return;
    }
    shutdownLatched = true;

    inputs.enabled  = false;
    inputs.throttle = 0.0f;
    inputs.brake    = 0.0f;
    controller.reset();
    vesc.sendCurrent(0.0f);
    Serial.println(F("Power off — releasing power hold."));

    mcp.digitalWrite(MCP_PWR_HOLD_PIN, LOW);
}

void power_poll(uint32_t nowMs,
                 scooter::CurrentControlInputs &inputs,
                 scooter::CurrentController &controller,
                 scooter::VescUart &vesc)
{
    const bool pressed = mcp.digitalRead(MCP_PWR_BTN_PIN) == HIGH;
    if (pressed && !wasPressed) {
        pressedSinceMs = nowMs;
    }
    wasPressed = pressed;

    if (pressed && (nowMs - pressedSinceMs >= POWER_BUTTON_SHUTDOWN_HOLD_MS)) {
        Serial.println(F("Power button held — shutting down."));
        power_shutdown(inputs, controller, vesc);
    }
}
