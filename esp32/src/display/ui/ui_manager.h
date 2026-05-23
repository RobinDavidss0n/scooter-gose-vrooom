#pragma once
#include <lvgl.h>
#include "vesc/vesc_types.h"

// Forward declarations
class ScreenDashboard;
class ScreenSettings;

enum class Screen : uint8_t {
    DASHBOARD = 0,
    SETTINGS  = 1,
};

// ==============================================================================
// UIManager — owns all LVGL screens and routes data updates to the active one
// ==============================================================================
class UIManager {
public:
    static UIManager& instance() {
        static UIManager inst;
        return inst;
    }

    // Create all screens and load the dashboard.  Call after lv_init().
    void begin();

    // Push fresh telemetry to the currently visible screen.
    void update(const VescValues& v);

    // Propagate a mode change (from BLE / web) to the active dashboard.
    void setMode(int modeIdx);

    // Navigate to a screen (animated transition)
    void switchTo(Screen s);

    Screen currentScreen() const { return _current; }

    // Propagate connection info to the settings screen
    void setNetworkInfo(const char* ssid, const char* ip, const char* ble);
    void setVescFwVersion(uint8_t major, uint8_t minor, const char* hw);

private:
    UIManager() = default;

    Screen           _current = Screen::DASHBOARD;
    ScreenDashboard* _dashboard = nullptr;
    ScreenSettings*  _settings  = nullptr;
};
