#include "wifi_manager.h"

void WiFiManager::begin(const char* ssid, const char* password,
                        uint8_t channel, uint8_t max_clients) {
    WiFi.mode(WIFI_AP);

    bool ok = WiFi.softAP(ssid, password, channel, 0, max_clients);
    if (!ok) {
        Serial.println("[WiFi] softAP failed to start");
        return;
    }

    // Optional: set a static IP (default is 192.168.4.1)
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);

    _up = true;
    Serial.printf("[WiFi] AP started — SSID: %s  IP: %s\n", ssid, ip().c_str());
}
