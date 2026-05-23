#pragma once
#include <Arduino.h>
#include <WiFi.h>

// ==============================================================================
// WiFiManager — creates a WiFi Access Point so a phone can connect to the
// scooter's web dashboard without any external router.
// ==============================================================================

class WiFiManager {
public:
    // Start the AP. Call once in setup().
    void begin(const char* ssid, const char* password,
               uint8_t channel = 6, uint8_t max_clients = 3);

    // Returns the AP's IP address as a string (e.g. "192.168.4.1")
    String ip() const { return WiFi.softAPIP().toString(); }

    // Number of clients currently connected
    uint8_t clientCount() const { return WiFi.softAPgetStationNum(); }

    bool isUp() const { return _up; }

private:
    bool _up = false;
};
