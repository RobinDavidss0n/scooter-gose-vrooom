#pragma once

#include <Arduino.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Light state (defined in lights.cpp). Plain variables for now — no
// touch/console/etc. control scheme has been decided, so anything can poke
// these directly until that's settled.
// ---------------------------------------------------------------------------
extern bool    frontLightOn;
extern uint8_t rearLightIdleBrightness;
extern uint8_t rearLightBrakeBrightness;

// ---------------------------------------------------------------------------
// Configures the rear light's PWM channel and the front light's MCP23017
// output pin. Call once from setup(), after expander_init().
// ---------------------------------------------------------------------------
void lights_init();

// ---------------------------------------------------------------------------
// Applies the current light state to the hardware. Front light follows
// frontLightOn (plain on/off, via the expander); rear light sits at
// rearLightIdleBrightness and jumps to rearLightBrakeBrightness while
// braking. Call every loop tick.
// ---------------------------------------------------------------------------
void update_lights(bool braking);
