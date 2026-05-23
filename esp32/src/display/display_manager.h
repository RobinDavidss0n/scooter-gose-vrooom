#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "touch/cst816s.h"

// ==============================================================================
// DisplayManager — initialises GC9A01 + LVGL + CST816S touch
// Owns the draw buffer and provides LVGL flush/touch callbacks.
// Call update() in the main loop (or from a dedicated FreeRTOS task) at ~5 ms.
// ==============================================================================

class DisplayManager {
public:
    static DisplayManager& instance() {
        static DisplayManager inst;
        return inst;
    }

    // Initialise TFT, LVGL, and touch.  Call once in setup().
    void begin();

    // Drive LVGL timers — call every ~5 ms
    void update();

    // Access to touch driver (for gesture detection outside LVGL)
    CST816S& touch() { return _touch; }

    // Backlight control  (0–255)
    void setBrightness(uint8_t val);

private:
    DisplayManager() = default;

    TFT_eSPI _tft;
    CST816S  _touch;

    lv_disp_draw_buf_t _draw_buf;
    lv_color_t*        _buf1 = nullptr;
    lv_color_t*        _buf2 = nullptr;

    lv_disp_drv_t  _disp_drv;
    lv_indev_drv_t _indev_drv;

    // LVGL callbacks — must be static
    static void lvFlushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p);
    static void lvTouchCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
};
