#include "screen_settings.h"
#include "display/ui/ui_manager.h"
#include <Arduino.h>

static constexpr uint32_t S_BG   = 0x0a0a12;
static constexpr uint32_t S_ACC  = 0x00aaff;
static constexpr uint32_t S_DIM  = 0x666688;
static constexpr uint32_t S_WHT  = 0xffffff;

// ------------------------------------------------------------------------------
ScreenSettings::ScreenSettings() {
    buildLayout();
}

void ScreenSettings::show() {
    lv_scr_load_anim(_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

// ------------------------------------------------------------------------------
void ScreenSettings::update(const VescValues& v) {
    if (!_scr) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f Ah", v.amp_hours);
    lv_label_set_text(_lbl_ah, buf);

    snprintf(buf, sizeof(buf), "%.1f Wh", v.watt_hours);
    lv_label_set_text(_lbl_wh, buf);

    snprintf(buf, sizeof(buf), "%.1f Wh regen", v.watt_hours_charged);
    lv_label_set_text(_lbl_wh_regen, buf);
}

// ------------------------------------------------------------------------------
void ScreenSettings::setNetworkInfo(const char* ssid, const char* ip, const char* ble_name) {
    char buf[64];

    snprintf(buf, sizeof(buf), "WiFi: %s", ssid);
    lv_label_set_text(_lbl_ssid, buf);

    snprintf(buf, sizeof(buf), "IP: %s", ip);
    lv_label_set_text(_lbl_ip, buf);

    snprintf(buf, sizeof(buf), "BLE: %s", ble_name);
    lv_label_set_text(_lbl_ble, buf);
}

void ScreenSettings::setFwVersion(uint8_t major, uint8_t minor, const char* hw) {
    char buf[32];
    snprintf(buf, sizeof(buf), "VESC FW %d.%d  %s", major, minor, hw);
    lv_label_set_text(_lbl_fw, buf);
}

// ==============================================================================
// Layout
// ==============================================================================
void ScreenSettings::buildLayout() {
    _scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(S_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(_scr);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(S_ACC), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    // Divider line
    lv_obj_t* line = lv_obj_create(_scr);
    lv_obj_set_size(line, 160, 2);
    lv_obj_set_style_bg_color(line, lv_color_hex(S_ACC), LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 50);

    // Helper lambda to create a small info label pair (no std::function needed)
    auto makeLabel = [&](const char* text, int y_offset) -> lv_obj_t* {
        lv_obj_t* lbl = lv_label_create(_scr);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(S_WHT), 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y_offset);
        return lbl;
    };

    _lbl_ssid    = makeLabel("WiFi: —", 62);
    _lbl_ip      = makeLabel("IP: —", 78);
    _lbl_ble     = makeLabel("BLE: —", 94);
    _lbl_fw      = makeLabel("VESC FW —", 110);

    // Divider
    lv_obj_t* line2 = lv_obj_create(_scr);
    lv_obj_set_size(line2, 160, 1);
    lv_obj_set_style_bg_color(line2, lv_color_hex(S_DIM), LV_PART_MAIN);
    lv_obj_set_style_border_width(line2, 0, LV_PART_MAIN);
    lv_obj_align(line2, LV_ALIGN_TOP_MID, 0, 126);

    // Trip stats
    lv_obj_t* stats_title = lv_label_create(_scr);
    lv_label_set_text(stats_title, "TRIP");
    lv_obj_set_style_text_font(stats_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(stats_title, lv_color_hex(S_DIM), 0);
    lv_obj_align(stats_title, LV_ALIGN_TOP_MID, 0, 132);

    _lbl_ah       = makeLabel("— Ah", 146);
    _lbl_wh       = makeLabel("— Wh", 160);
    _lbl_wh_regen = makeLabel("— Wh regen", 174);

    // Back button
    _btn_back = lv_btn_create(_scr);
    lv_obj_set_size(_btn_back, 80, 32);
    lv_obj_align(_btn_back, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(_btn_back, lv_color_hex(S_ACC), LV_PART_MAIN);
    lv_obj_set_style_radius(_btn_back, 16, LV_PART_MAIN);
    lv_obj_add_event_cb(_btn_back, backBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_lbl = lv_label_create(_btn_back);
    lv_label_set_text(btn_lbl, "BACK");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(btn_lbl);
}

// ------------------------------------------------------------------------------
void ScreenSettings::backBtnCb(lv_event_t* e) {
    UIManager::instance().switchTo(Screen::DASHBOARD);
}
