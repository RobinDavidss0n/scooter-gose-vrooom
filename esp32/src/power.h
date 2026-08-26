#pragma once

#include <Arduino.h>

#include "control/CurrentController.h"
#include "vesc/VescUart.h"

// ---------------------------------------------------------------------------
// Soft power switch (see wiring/diagram.mmd — PWR_RELAY/PWR_LATCH). The
// handlebar button bootstraps the relay entirely in hardware; this module is
// firmware's side of holding it closed, sensing the same button once running,
// and releasing the latch on shutdown.
// ---------------------------------------------------------------------------

// Asserts the hold line so the relay stays closed once the MCU has booted,
// and configures the local button-sense pin. Call once from setup(), after
// expander_init().
void power_init();

// Debounces the power button; once held continuously for
// POWER_BUTTON_SHUTDOWN_HOLD_MS, triggers power_shutdown(). A short tap does
// nothing — deliberate, since an accidental read shouldn't cut power while
// riding. Call every loop tick.
void power_poll(uint32_t nowMs,
                 scooter::CurrentControlInputs &inputs,
                 scooter::CurrentController &controller,
                 scooter::VescUart &vesc);

// Zeros throttle/brake, stops the VESC, and releases the hold line — cutting
// power to the VESC and, a moment later, the MCU itself. Safe to call
// directly (e.g. from the "power off" console command) for an immediate
// shutdown with no hold required; idempotent if power_poll() already
// triggered it.
void power_shutdown(scooter::CurrentControlInputs &inputs,
                     scooter::CurrentController &controller,
                     scooter::VescUart &vesc);
