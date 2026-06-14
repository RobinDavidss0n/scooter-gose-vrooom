#pragma once

#include <lvgl.h>

#include "vesc/VescUart.h"

// ---------------------------------------------------------------------------
// Initialises the LCD hardware and the LVGL display driver, then builds the
// initial widget tree. Call once from setup().
// ---------------------------------------------------------------------------
void setup_ui();

// ---------------------------------------------------------------------------
// Refreshes all on-screen widgets and drives the comms-warning flash logic.
// Call at UI_INTERVAL_MS from loop().
//
// Comms warning: a flashing "! COMMS" label appears at the top of the screen
// whenever the VESC telemetry is stale (hasFreshTelemetry() == false). This
// covers both full-offline and intermittent packet-loss scenarios.
// ---------------------------------------------------------------------------
void update_ui(uint32_t nowMs, const scooter::VescUart &vesc);

// ---------------------------------------------------------------------------
// LVGL display-flush callback (registered during setup_ui).
// ---------------------------------------------------------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
