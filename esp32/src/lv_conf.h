#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*
 * Minimal project-local LVGL config shim.
 *
 * We intentionally keep this file small. LVGL's lv_conf_internal.h provides
 * defaults for anything not defined here. Add project-specific overrides as
 * the UI grows.
 */

// Fonts — enable sizes used in the UI
#define LV_FONT_MONTSERRAT_14  1  // LVGL default; kept for fallback
#define LV_FONT_MONTSERRAT_48  1  // Used for the speedometer readout

// Match LVGL's framebuffer format to the GC9A01 panel write path.
// main.cpp flushes pixels as RGB565 via LovyanGFX, so make that explicit.
#define LV_COLOR_DEPTH         16

// Disable the built-in default theme.
// Without it, LVGL uses a transparent screen background, which lets
// the hardware fill (black) show through. Widget colors are set explicitly.
#define LV_USE_THEME_DEFAULT   0

#endif