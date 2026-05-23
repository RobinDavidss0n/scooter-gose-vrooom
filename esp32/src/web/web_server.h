#pragma once
#include <Arduino.h>
#include <functional>
#include <ESPAsyncWebServer.h>
#include <freertos/semphr.h>
#include "vesc/vesc_types.h"

// ==============================================================================
// WebServer — serves the phone dashboard over WiFi AP
//
//  GET  /             → index.html (from SPIFFS)
//  GET  /api/status   → JSON snapshot of current VescValues
//  GET  /api/mode?m=  → switch ride mode (eco / sport / turbo)
//  POST /api/set      → JSON body: { "max_current": float }
//  WS   /ws           → WebSocket; server pushes JSON every WS_PUSH_MS
// ==============================================================================

class ScooterWebServer {
public:
    // server_port : TCP port (typically 80)
    // shared_vals : pointer to the shared VescValues struct
    // mutex       : FreeRTOS mutex protecting shared_vals
    void begin(uint16_t server_port, VescValues* shared_vals, SemaphoreHandle_t mutex);

    // Call from the main loop — pushes a telemetry frame over WebSocket
    // if enough time has passed since the last push.
    void tick();

    // Update internally cached values (called by the VESC task)
    void setValues(const VescValues& v);

    // Register callback for when /api/mode is called (0=ECO,1=SPORT,2=TURBO).
    void setModeCallback(std::function<void(int)> cb) { _modeCallback = cb; }

    // Pointer to the global mode index — included in /api/status JSON response.
    void setModeIndexPtr(volatile int* ptr) { _modeIdx = ptr; }

private:
    AsyncWebServer*  _server  = nullptr;
    AsyncWebSocket*  _ws      = nullptr;

    VescValues*        _vals   = nullptr;
    SemaphoreHandle_t  _mutex  = nullptr;
    volatile int*      _modeIdx = nullptr;       // points to gModeIdx in main.cpp

    std::function<void(int)> _modeCallback;      // called when /api/mode fires

    uint32_t _last_push_ms = 0;

    // Serialise VescValues to JSON string
    String buildJson(const VescValues& v);

    // WebSocket event handler (static)
    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
};
