#include "ui_manager.h"
#include "screens/screen_dashboard.h"
#include "screens/screen_settings.h"
#include <cstring>

void UIManager::begin() {
    _dashboard = new ScreenDashboard();
    _settings  = new ScreenSettings();
    switchTo(Screen::DASHBOARD);
}

void UIManager::update(const VescValues& v) {
    if (_current == Screen::DASHBOARD && _dashboard) _dashboard->update(v);
    if (_current == Screen::SETTINGS  && _settings)  _settings->update(v);
}

void UIManager::setMode(int modeIdx) {
    if (_dashboard) _dashboard->setMode(modeIdx);
}

void UIManager::setMode(int modeIdx) {
    if (_dashboard) _dashboard->setMode(modeIdx);
}

void UIManager::switchTo(Screen s) {
    _current = s;
    switch (s) {
        case Screen::DASHBOARD: if (_dashboard) _dashboard->show(); break;
        case Screen::SETTINGS:  if (_settings)  _settings->show();  break;
    }
}

void UIManager::setNetworkInfo(const char* ssid, const char* ip, const char* ble) {
    if (_settings) _settings->setNetworkInfo(ssid, ip, ble);
}

void UIManager::setVescFwVersion(uint8_t major, uint8_t minor, const char* hw) {
    if (_settings) _settings->setFwVersion(major, minor, hw);
}
