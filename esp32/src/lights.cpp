#include "lights.h"

#include "expander.h"

bool    frontLightOn             = false;
uint8_t rearLightIdleBrightness  = REAR_LIGHT_DEFAULT_IDLE_BRIGHTNESS;
uint8_t rearLightBrakeBrightness = REAR_LIGHT_DEFAULT_BRAKE_BRIGHTNESS;

namespace {

const int     kRearLightPin        = REAR_LIGHT_PIN;
const uint8_t kRearLightPwmChannel = REAR_LIGHT_PWM_CHANNEL;

}  // namespace

void lights_init()
{
    ledcSetup(kRearLightPwmChannel, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RESOLUTION_BITS);
    ledcAttachPin(kRearLightPin, kRearLightPwmChannel);
    ledcWrite(kRearLightPwmChannel, 0);

    mcp.pinMode(MCP_FRONT_LIGHT_PIN, OUTPUT);
    mcp.digitalWrite(MCP_FRONT_LIGHT_PIN, LOW);
}

void update_lights(bool braking)
{
    mcp.digitalWrite(MCP_FRONT_LIGHT_PIN, frontLightOn ? HIGH : LOW);
    ledcWrite(kRearLightPwmChannel, braking ? rearLightBrakeBrightness : rearLightIdleBrightness);
}
