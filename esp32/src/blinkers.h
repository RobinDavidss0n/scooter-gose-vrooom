#pragma once

#include <Arduino.h>

#include "config.h"
#include "control/CurrentController.h"

// ---------------------------------------------------------------------------
// L/R buttons + blinker LEDs live on an MCP23017 I2C expander (see
// wiring/diagram.mmd). Normally the buttons toggle the blinkers; while
// unrestrictedModeActive is set, the same presses instead nudge the current
// control profile's speed limit up/down and the blinkers stay off.
// ---------------------------------------------------------------------------
extern bool unrestrictedModeActive;

// ---------------------------------------------------------------------------
// Configures the MCP23017 over I2C. Call once from setup().
// ---------------------------------------------------------------------------
void blinkers_init();

// ---------------------------------------------------------------------------
// Debounces the buttons, dispatches them per unrestrictedModeActive, and
// drives the blink timing. Call every loop tick.
// ---------------------------------------------------------------------------
void blinkers_poll(uint32_t nowMs, scooter::CurrentControlProfile &profile);
