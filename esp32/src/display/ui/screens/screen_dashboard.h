#pragma once
#include <lvgl.h>
#include "vesc/vesc_types.h"

// ==============================================================================
// ScreenDashboard — the main riding display
//
// Layout (240×240 round display):
//
//          ┌──────── Mode label ─────────┐
//         ╱  Speed arc (outer ring)       ╲
//        │  ┌────── Battery arc ──────┐    │
//        │  │   [SPEED km/h]          │    │
//        │  │   fault indicator       │    │
//        │  └─────────────────────────┘    │
//         ╲  [°C FET] [A] [V] [°C MOT]  ╱
//          └────────────────────────────┘
//
//  Touch gestures:
//    - Swipe left/right → cycle mode (ECO/SPORT/TURBO)
//    - Long press       → navigate to Settings screen
// ==============================================================================

class ScreenDashboard {
public:
    ScreenDashboard();

    // Load / show this screen
    void show();

    // Push new telemetry values
    void update(const VescValues& v);

    // Force mode badge + arc colour (called when BLE/web changes the mode)
    void setMode(int modeIdx);

    lv_obj_t* screen() { return _scr; }

private:
    lv_obj_t* _scr          = nullptr;

    // Speed elements
    lv_obj_t* _arc_speed    = nullptr;   // outer arc
    lv_obj_t* _lbl_speed    = nullptr;   // big speed number
    lv_obj_t* _lbl_unit     = nullptr;   // "km/h"

    // Battery arc (inner ring)
    lv_obj_t* _arc_bat      = nullptr;
    lv_obj_t* _lbl_bat      = nullptr;   // e.g. "74%"

    // Status row (bottom)
    lv_obj_t* _lbl_temp_fet = nullptr;
    lv_obj_t* _lbl_current  = nullptr;
    lv_obj_t* _lbl_voltage  = nullptr;
    lv_obj_t* _lbl_power    = nullptr;

    // Mode + fault
    lv_obj_t* _lbl_mode     = nullptr;
    lv_obj_t* _lbl_fault    = nullptr;

    // Connection indicator
    lv_obj_t* _led_conn     = nullptr;

    int _last_mode_idx = 1;  // default SPORT

    void buildLayout();
    void applyModeColour(uint32_t colour);

    static void touchEventCb(lv_event_t* e);
};
