#include "lights.h"

bool    frontLightOn             = false;
uint8_t frontLightBrightness     = FRONT_LIGHT_DEFAULT_BRIGHTNESS;
uint8_t rearLightIdleBrightness  = REAR_LIGHT_DEFAULT_IDLE_BRIGHTNESS;
uint8_t rearLightBrakeBrightness = REAR_LIGHT_DEFAULT_BRAKE_BRIGHTNESS;

namespace {

struct LightChannel {
    int     pin;
    uint8_t pwmChannel;
};

const LightChannel kFrontLight = { FRONT_LIGHT_PIN, FRONT_LIGHT_PWM_CHANNEL };
const LightChannel kRearLight  = { REAR_LIGHT_PIN,  REAR_LIGHT_PWM_CHANNEL };

// Shared helpers so front/rear setup and writes don't duplicate LEDC calls.
void configure_light(const LightChannel &light)
{
    ledcSetup(light.pwmChannel, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RESOLUTION_BITS);
    ledcAttachPin(light.pin, light.pwmChannel);
}

void write_light(const LightChannel &light, uint8_t brightness)
{
    ledcWrite(light.pwmChannel, brightness);
}

}  // namespace

void lights_init()
{
    configure_light(kFrontLight);
    configure_light(kRearLight);
    write_light(kFrontLight, 0);
    write_light(kRearLight, 0);
}

void update_lights(bool braking)
{
    write_light(kFrontLight, frontLightOn ? frontLightBrightness : 0);
    write_light(kRearLight, braking ? rearLightBrakeBrightness : rearLightIdleBrightness);
}
