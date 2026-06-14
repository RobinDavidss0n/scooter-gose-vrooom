#pragma once

#include <Arduino.h>

#include "config.h"
#include "control/CurrentController.h"
#include "vesc/VescUart.h"

namespace scooter {

// ---------------------------------------------------------------------------
// Bundles the shared state the console needs to read/modify. All fields are
// non-const references so commands can update them live.
// ---------------------------------------------------------------------------
struct ConsoleContext {
    CurrentControlInputs  &inputs;
    CurrentControlProfile &profile;
    CurrentControlOutput  &output;
    CurrentController     &controller;
    VescUart              &vesc;
    bool                  &throttleLogEnabled;
};

}  // namespace scooter

// ---------------------------------------------------------------------------
// Print the list of available commands.
// ---------------------------------------------------------------------------
void print_console_help();

// ---------------------------------------------------------------------------
// Print a full status snapshot to Serial.
// ---------------------------------------------------------------------------
void print_status(scooter::ConsoleContext &ctx, uint32_t nowMs);

// ---------------------------------------------------------------------------
// Drain Serial input and execute any complete commands.
// ---------------------------------------------------------------------------
void poll_console(scooter::ConsoleContext &ctx);
