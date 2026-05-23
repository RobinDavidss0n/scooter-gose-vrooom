#pragma once
#include <lvgl.h>
#include "vesc/vesc_types.h"

// ==============================================================================
// ScreenSettings — system information and basic configuration
//
// Shows:
//  - WiFi SSID and IP address
//  - BLE device name
//  - VESC firmware version
//  - Trip stats: km, Wh consumed, regen Wh
//  - Cumulative amp-hours
//  - "Back" tap → Dashboard
// ==============================================================================

class ScreenSettings {
public:
    ScreenSettings();

    void show();
    void update(const VescValues& v);

    // Call once when WiFi/BLE are up to fill the connection info labels
    void setNetworkInfo(const char* ssid, const char* ip, const char* ble_name);
    void setFwVersion(uint8_t major, uint8_t minor, const char* hw);

    lv_obj_t* screen() { return _scr; }

private:
    lv_obj_t* _scr         = nullptr;

    lv_obj_t* _lbl_ssid    = nullptr;
    lv_obj_t* _lbl_ip      = nullptr;
    lv_obj_t* _lbl_ble     = nullptr;
    lv_obj_t* _lbl_fw      = nullptr;

    lv_obj_t* _lbl_ah      = nullptr;
    lv_obj_t* _lbl_wh      = nullptr;
    lv_obj_t* _lbl_wh_regen = nullptr;

    lv_obj_t* _btn_back    = nullptr;

    void buildLayout();
    static void backBtnCb(lv_event_t* e);
};
