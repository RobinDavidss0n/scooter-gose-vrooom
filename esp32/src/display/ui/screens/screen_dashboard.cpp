#include "screen_dashboard.h"
#include "display/ui/ui_manager.h"
#include "config.h"
#include <Arduino.h>
#include <stdio.h>

// Colour palette
static constexpr uint32_t COL_BG        = 0x0a0a12;
static constexpr uint32_t COL_RING_BG   = 0x1a1a2e;
static constexpr uint32_t COL_ECO       = 0x00c48c;
static constexpr uint32_t COL_SPORT     = 0x00aaff;
static constexpr uint32_t COL_TURBO     = 0xff4444;
static constexpr uint32_t COL_BAT_GOOD  = 0x44dd88;
static constexpr uint32_t COL_BAT_MED   = 0xffaa00;
static constexpr uint32_t COL_BAT_LOW   = 0xff3333;
static constexpr uint32_t COL_TEXT_DIM  = 0x666688;
static constexpr uint32_t COL_WHITE     = 0xffffff;

// Max display speed for arc scale
static constexpr int SPEED_ARC_MAX = 35;   // km/h at full arc

// ------------------------------------------------------------------------------
ScreenDashboard::ScreenDashboard() {
    buildLayout();
}

// ------------------------------------------------------------------------------
void ScreenDashboard::show() {
    lv_scr_load_anim(_scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

// ------------------------------------------------------------------------------
void ScreenDashboard::update(const VescValues& v) {
    if (!_scr) return;

    // --- Speed arc ---
    int speed_int = (int)(v.speed_kmh + 0.5f);
    speed_int = constrain(speed_int, 0, SPEED_ARC_MAX);
    lv_arc_set_value(_arc_speed, speed_int);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", speed_int);
    lv_label_set_text(_lbl_speed, buf);

    // Speed arc colour: green < 20, yellow 20-28, red > 28
    uint32_t sc = (speed_int < 20) ? COL_ECO : (speed_int < 28) ? COL_BAT_MED : COL_TURBO;
    lv_obj_set_style_arc_color(_arc_speed, lv_color_hex(sc), LV_PART_INDICATOR);

    // --- Battery arc ---
    int bat_pct = constrain(v.battery_pct, 0, 100);
    lv_arc_set_value(_arc_bat, bat_pct);
    snprintf(buf, sizeof(buf), "%d%%", bat_pct);
    lv_label_set_text(_lbl_bat, buf);

    uint32_t bc = (bat_pct > 40) ? COL_BAT_GOOD : (bat_pct > 15) ? COL_BAT_MED : COL_BAT_LOW;
    lv_obj_set_style_arc_color(_arc_bat, lv_color_hex(bc), LV_PART_INDICATOR);

    // --- Status row ---
    snprintf(buf, sizeof(buf), "%.0f°C", v.temp_mos);
    lv_label_set_text(_lbl_temp_fet, buf);

    snprintf(buf, sizeof(buf), "%.1fA", fabsf(v.avg_input_current));
    lv_label_set_text(_lbl_current, buf);

    snprintf(buf, sizeof(buf), "%.1fV", v.v_in);
    lv_label_set_text(_lbl_voltage, buf);

    snprintf(buf, sizeof(buf), "%.0fW", fabsf(v.power_w));
    lv_label_set_text(_lbl_power, buf);

    // --- Fault ---
    if (v.fault_code != FAULT_CODE_NONE) {
        lv_label_set_text(_lbl_fault, faultString(v.fault_code));
        lv_obj_clear_flag(_lbl_fault, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_lbl_fault, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Connection LED ---
    if (v.valid) {
        lv_led_set_color(_led_conn, lv_color_hex(COL_BAT_GOOD));
        lv_led_on(_led_conn);
    } else {
        lv_led_set_color(_led_conn, lv_color_hex(COL_TURBO));
        lv_led_off(_led_conn);
    }
}

// ==============================================================================
// Layout builder
// ==============================================================================
void ScreenDashboard::buildLayout() {
    _scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ------------------------------------------------------------------
    // Outer speed arc (220×220, full 270° sweep, start bottom-left)
    // ------------------------------------------------------------------
    _arc_speed = lv_arc_create(_scr);
    lv_obj_set_size(_arc_speed, 220, 220);
    lv_obj_center(_arc_speed);
    lv_arc_set_rotation(_arc_speed, 135);        // start at 7 o'clock
    lv_arc_set_bg_angles(_arc_speed, 0, 270);    // 270° sweep
    lv_arc_set_range(_arc_speed, 0, SPEED_ARC_MAX);
    lv_arc_set_value(_arc_speed, 0);
    lv_obj_set_style_arc_color(_arc_speed, lv_color_hex(COL_RING_BG), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc_speed, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc_speed, lv_color_hex(COL_SPORT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_arc_speed, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(_arc_speed, true, LV_PART_INDICATOR);
    lv_obj_remove_style(_arc_speed, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc_speed, LV_OBJ_FLAG_CLICKABLE);

    // ------------------------------------------------------------------
    // Inner battery arc (190×190)
    // ------------------------------------------------------------------
    _arc_bat = lv_arc_create(_scr);
    lv_obj_set_size(_arc_bat, 190, 190);
    lv_obj_center(_arc_bat);
    lv_arc_set_rotation(_arc_bat, 135);
    lv_arc_set_bg_angles(_arc_bat, 0, 270);
    lv_arc_set_range(_arc_bat, 0, 100);
    lv_arc_set_value(_arc_bat, 100);
    lv_obj_set_style_arc_color(_arc_bat, lv_color_hex(COL_RING_BG), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc_bat, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc_bat, lv_color_hex(COL_BAT_GOOD), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_arc_bat, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(_arc_bat, true, LV_PART_INDICATOR);
    lv_obj_remove_style(_arc_bat, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_arc_bat, LV_OBJ_FLAG_CLICKABLE);

    // ------------------------------------------------------------------
    // Large speed number — centred, slightly above middle
    // ------------------------------------------------------------------
    _lbl_speed = lv_label_create(_scr);
    lv_label_set_text(_lbl_speed, "0");
    lv_obj_set_style_text_font(_lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lbl_speed, lv_color_hex(COL_WHITE), 0);
    lv_obj_align(_lbl_speed, LV_ALIGN_CENTER, 0, -18);

    // "km/h" unit label
    _lbl_unit = lv_label_create(_scr);
    lv_label_set_text(_lbl_unit, "km/h");
    lv_obj_set_style_text_font(_lbl_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lbl_unit, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(_lbl_unit, LV_ALIGN_CENTER, 0, 28);

    // ------------------------------------------------------------------
    // Mode label — top centre
    // ------------------------------------------------------------------
    _lbl_mode = lv_label_create(_scr);
    lv_label_set_text(_lbl_mode, "SPORT");
    lv_obj_set_style_text_font(_lbl_mode, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lbl_mode, lv_color_hex(COL_SPORT), 0);
    lv_obj_align(_lbl_mode, LV_ALIGN_CENTER, 0, -68);

    // ------------------------------------------------------------------
    // Battery % label — below unit label
    // ------------------------------------------------------------------
    _lbl_bat = lv_label_create(_scr);
    lv_label_set_text(_lbl_bat, "100%");
    lv_obj_set_style_text_font(_lbl_bat, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_bat, lv_color_hex(COL_BAT_GOOD), 0);
    lv_obj_align(_lbl_bat, LV_ALIGN_CENTER, 0, 50);

    // ------------------------------------------------------------------
    // Status row — four labels at the bottom of the circle
    // ------------------------------------------------------------------
    const int STATUS_Y = 82;
    const int STATUS_SPACING = 50;

    _lbl_temp_fet = lv_label_create(_scr);
    lv_label_set_text(_lbl_temp_fet, "--°C");
    lv_obj_set_style_text_font(_lbl_temp_fet, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_temp_fet, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(_lbl_temp_fet, LV_ALIGN_CENTER, -STATUS_SPACING - 10, STATUS_Y);

    _lbl_current = lv_label_create(_scr);
    lv_label_set_text(_lbl_current, "--A");
    lv_obj_set_style_text_font(_lbl_current, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_current, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(_lbl_current, LV_ALIGN_CENTER, -10, STATUS_Y);

    _lbl_voltage = lv_label_create(_scr);
    lv_label_set_text(_lbl_voltage, "--V");
    lv_obj_set_style_text_font(_lbl_voltage, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_voltage, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(_lbl_voltage, LV_ALIGN_CENTER, 30, STATUS_Y);

    _lbl_power = lv_label_create(_scr);
    lv_label_set_text(_lbl_power, "--W");
    lv_obj_set_style_text_font(_lbl_power, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_power, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(_lbl_power, LV_ALIGN_CENTER, STATUS_SPACING + 10, STATUS_Y);

    // ------------------------------------------------------------------
    // Fault label (hidden unless fault is active) — centre bottom
    // ------------------------------------------------------------------
    _lbl_fault = lv_label_create(_scr);
    lv_label_set_text(_lbl_fault, "FAULT");
    lv_obj_set_style_text_font(_lbl_fault, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_fault, lv_color_hex(COL_TURBO), 0);
    lv_obj_align(_lbl_fault, LV_ALIGN_CENTER, 0, 65);
    lv_obj_add_flag(_lbl_fault, LV_OBJ_FLAG_HIDDEN);

    // ------------------------------------------------------------------
    // Connection LED — small dot, top-right
    // ------------------------------------------------------------------
    _led_conn = lv_led_create(_scr);
    lv_obj_set_size(_led_conn, 10, 10);
    lv_obj_align(_led_conn, LV_ALIGN_TOP_RIGHT, -30, 30);
    lv_led_set_color(_led_conn, lv_color_hex(COL_TURBO));
    lv_led_off(_led_conn);

    // ------------------------------------------------------------------
    // Touch event — swipe to change mode, long press → settings
    // ------------------------------------------------------------------
    // Gestures go to the screen by default in LVGL 8.x when no child
    // object consumes them.  No extra flags needed on the screen itself.
    lv_obj_add_event_cb(_scr, touchEventCb, LV_EVENT_GESTURE,      this);
    lv_obj_add_event_cb(_scr, touchEventCb, LV_EVENT_LONG_PRESSED, this);
}

// ------------------------------------------------------------------------------
void ScreenDashboard::setMode(int modeIdx) {
    modeIdx = constrain(modeIdx, 0, RIDE_MODE_COUNT - 1);
    if (modeIdx == _last_mode_idx) return;   // no change
    _last_mode_idx = modeIdx;
    const RideMode& m = RIDE_MODES[modeIdx];
    if (_lbl_mode) lv_label_set_text(_lbl_mode, m.name);
    applyModeColour(m.color);
}

void ScreenDashboard::applyModeColour(uint32_t colour) {
    lv_obj_set_style_arc_color(_arc_speed, lv_color_hex(colour), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(_lbl_mode, lv_color_hex(colour), 0);
}

// ------------------------------------------------------------------------------
void ScreenDashboard::touchEventCb(lv_event_t* e) {
    auto* self = static_cast<ScreenDashboard*>(lv_event_get_user_data(e));

    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        UIManager::instance().switchTo(Screen::SETTINGS);
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        int next = self->_last_mode_idx;

        if (dir == LV_DIR_LEFT)       next = min(next + 1, RIDE_MODE_COUNT - 1);
        else if (dir == LV_DIR_RIGHT) next = max(next - 1, 0);

        if (next != self->_last_mode_idx) {
            self->_last_mode_idx = next;
            const RideMode& m = RIDE_MODES[next];
            lv_label_set_text(self->_lbl_mode, m.name);
            self->applyModeColour(m.color);
        }
    }
}
