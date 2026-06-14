#include "ui.h"

#include <Arduino.h>
#include <math.h>
#include <lvgl.h>

#include "display.h"
#include "config.h"

// Uncomment to enable human-readable UI diagnostics on Serial.
// #define UI_DEBUG

// ---------------------------------------------------------------------------
// Hardware display instance (used by setup_ui and my_disp_flush)
// ---------------------------------------------------------------------------
static LGFX_Waveshare_128 s_lcd;

// ---------------------------------------------------------------------------
// LVGL framebuffer
// ---------------------------------------------------------------------------
static constexpr uint16_t kScreenW = 240;
static constexpr uint16_t kScreenH = 240;

static lv_disp_draw_buf_t s_drawBuf;
static lv_color_t         s_drawBufData[kScreenW * 10];

// ---------------------------------------------------------------------------
// Widget handles
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen       = nullptr;
static lv_obj_t *s_speedLabel   = nullptr;
static lv_obj_t *s_warnLabel    = nullptr;  // comms-warning flash label

// ---------------------------------------------------------------------------
// LVGL display flush
// ---------------------------------------------------------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    s_lcd.startWrite();
    s_lcd.setAddrWindow(area->x1, area->y1, w, h);
    s_lcd.writePixels(reinterpret_cast<uint16_t *>(&color_p->full), w * h, true);
    s_lcd.endWrite();
    lv_disp_flush_ready(disp);
}

// ---------------------------------------------------------------------------
// Screen setup
// ---------------------------------------------------------------------------
void setup_ui()
{
#ifdef UI_DEBUG
    Serial.println("[UI] setup_ui");
#endif

    // --- Hardware LCD init ---
    s_lcd.init();
    s_lcd.fillScreen(0x0000);  // Physical clear to black before LVGL takes over

    // --- LVGL init ---
    lv_init();
    lv_disp_draw_buf_init(&s_drawBuf, s_drawBufData, nullptr, kScreenW * 10);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res  = kScreenW;
    dispDrv.ver_res  = kScreenH;
    dispDrv.flush_cb = my_disp_flush;
    dispDrv.draw_buf = &s_drawBuf;
    lv_disp_drv_register(&dispDrv);

    // --- Root screen ---
    s_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(s_screen);

    // --- Speedometer label (centre) ---
    s_speedLabel = lv_label_create(s_screen);
    lv_obj_set_width(s_speedLabel, 180);
    lv_label_set_long_mode(s_speedLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_speedLabel, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_align(s_speedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_speedLabel, lv_color_white(), 0);
    lv_label_set_text(s_speedLabel, "...");
    lv_obj_align(s_speedLabel, LV_ALIGN_CENTER, 0, 0);

    // --- Comms-warning label (top arc area, hidden by default) ---
    // The round 240 x 240 display has its visible circle clipped by hardware,
    // so we position this a little below the top edge to stay in view.
    s_warnLabel = lv_label_create(s_screen);
    lv_obj_set_width(s_warnLabel, 160);
    lv_label_set_long_mode(s_warnLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_warnLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_warnLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_warnLabel, lv_color_make(255, 140, 0), 0);  // orange
    lv_label_set_text(s_warnLabel, "! COMMS");
    lv_obj_align(s_warnLabel, LV_ALIGN_TOP_MID, 0, 38);  // ~38 px below top stays inside circle
    lv_obj_add_flag(s_warnLabel, LV_OBJ_FLAG_HIDDEN);     // hidden until a warning fires
}

// ---------------------------------------------------------------------------
// Per-frame UI update
// ---------------------------------------------------------------------------
void update_ui(uint32_t nowMs, const scooter::VescUart &vesc)
{
    if (s_speedLabel == nullptr) {
        return;
    }

    // --- Comms-warning flash ---
    // Show whenever telemetry is stale OR a frame error was seen recently
    // (bad start/stop byte, CRC mismatch, oversize frame, short payload).
    // This catches intermittent bit errors even when some good packets still
    // get through. Flash at ~1.5 Hz: visible for 333 ms, hidden for 333 ms.
    if (!vesc.hasFreshTelemetry(nowMs) || vesc.hasRecentRxError(nowMs)) {
        const bool visible = (nowMs % 666u) < 333u;
        if (visible) {
            lv_obj_clear_flag(s_warnLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_warnLabel, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(s_warnLabel, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Speed / offline label ---
    if (!vesc.isConnected(nowMs)) {
        if (strcmp(lv_label_get_text(s_speedLabel), "OFFLINE") != 0) {
#ifdef UI_DEBUG
            Serial.println("[UI] state -> OFFLINE");
#endif
            lv_label_set_text(s_speedLabel, "OFFLINE");
        }
        lv_obj_invalidate(s_speedLabel);
        return;
    }

    const float kmh = fabsf(static_cast<float>(vesc.telemetry().rpm) * kRpmToKmhFactor);
    char text[16];
    snprintf(text, sizeof(text), "%d\nkm/h", static_cast<int>(kmh));
    if (strcmp(lv_label_get_text(s_speedLabel), text) != 0) {
#ifdef UI_DEBUG
        Serial.printf("[UI] state -> %s\n", text);
#endif
        lv_label_set_text(s_speedLabel, text);
    }
    lv_obj_invalidate(s_speedLabel);
}
