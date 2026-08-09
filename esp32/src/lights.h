#pragma once

#include <Arduino.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Light state (defined in lights.cpp). Plain variables for now — no
// touch/console/etc. control scheme has been decided, so anything can poke
// these directly until that's settled.
// ---------------------------------------------------------------------------
extern bool    frontLightOn;
extern uint8_t frontLightBrightness;
extern uint8_t rearLightIdleBrightness;
extern uint8_t rearLightBrakeBrightness;

// ---------------------------------------------------------------------------
// Configures the PWM channels for both lights. Call once from setup().
// ---------------------------------------------------------------------------
void lights_init();

// ---------------------------------------------------------------------------
// Applies the current light state to the hardware. Front light follows
// frontLightOn/frontLightBrightness; rear light sits at rearLightIdleBrightness
// and jumps to rearLightBrakeBrightness while braking. Call every loop tick.
// ---------------------------------------------------------------------------
void update_lights(bool braking);
