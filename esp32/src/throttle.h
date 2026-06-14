#pragma once

#include <Arduino.h>

#include "config.h"
#include "control/CurrentController.h"

// ---------------------------------------------------------------------------
// Throttle ADC state (defined in throttle.cpp)
// ---------------------------------------------------------------------------
extern int      throttleRawAdc;
extern uint16_t throttleMillivolts;
extern float    throttleNormalized;
extern bool     throttleLogEnabled;
extern uint32_t lastThrottleLogMs;

// ---------------------------------------------------------------------------
// Reads the physical throttle ADC and, if the controller is enabled,
// writes the normalized value into inputs.throttle.
// ---------------------------------------------------------------------------
void read_physical_throttle(scooter::CurrentControlInputs &inputs);

// ---------------------------------------------------------------------------
// Prints a throttle status line to Serial at most once per
// THROTTLE_LOG_INTERVAL_MS when the throttle is above the idle threshold.
// ---------------------------------------------------------------------------
void log_physical_throttle(uint32_t nowMs, bool controlEnabled);

// ---------------------------------------------------------------------------
// Maps a raw millivolt reading to [0.0, 1.0] with idle dead-zone.
// ---------------------------------------------------------------------------
float normalize_throttle_mv(uint16_t millivolts);
