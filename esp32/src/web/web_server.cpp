#include "web_server.h"
#include "config.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ==============================================================================
void ScooterWebServer::begin(uint16_t port, VescValues* shared_vals, SemaphoreHandle_t mutex) {
    _vals  = shared_vals;
    _mutex = mutex;

    if (!SPIFFS.begin(true)) {
        Serial.println("[WEB] SPIFFS mount failed — no web files available");
    } else {
        Serial.println("[WEB] SPIFFS mounted OK");
    }

    _server = new AsyncWebServer(port);
    _ws     = new AsyncWebSocket("/ws");

    // --- WebSocket -----------------------------------------------------------
    _ws->onEvent(onWsEvent);
    _server->addHandler(_ws);

    // --- Static files from SPIFFS --------------------------------------------
    _server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // --- REST: GET /api/status -----------------------------------------------
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        VescValues snap;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            snap = *_vals;
            xSemaphoreGive(_mutex);
        }
        AsyncResponseStream* resp = req->beginResponseStream("application/json");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->print(buildJson(snap));
        req->send(resp);
    });

    // --- REST: GET /api/mode?m=eco|sport|turbo --------------------------------
    // --- REST: GET /api/mode?m=eco|sport|turbo ---------------------------------
    _server->on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("m")) {
            req->send(400, "application/json", R"({"error":"missing param m"})");
            return;
        }
        String m = req->getParam("m")->value();
        m.toLowerCase();

        int newIdx = -1;
        if (m == "eco")   newIdx = 0;
        else if (m == "sport") newIdx = 1;
        else if (m == "turbo") newIdx = 2;

        if (newIdx < 0) {
            req->send(400, "application/json", R"({"error":"unknown mode"})");
            return;
        }

        if (_modeCallback) _modeCallback(newIdx);

        req->send(200, "application/json",
                  String(R"({"ok":true,"mode_idx":)") + newIdx + "}");
    });

    // --- REST: POST /api/set --------------------------------------------------
    _server->on("/api/set", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            // Future: apply settings. For now, acknowledge.
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // --- 404 handler ---------------------------------------------------------
    _server->onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    _server->begin();
    Serial.printf("[WEB] HTTP server started on port %d\n", port);
}

// ------------------------------------------------------------------------------
void ScooterWebServer::tick() {
    if (!_ws || !_vals) return;

    uint32_t now = millis();
    if (now - _last_push_ms < WS_PUSH_MS) return;
    _last_push_ms = now;

    if (_ws->count() == 0) return;

    VescValues snap;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        snap = *_vals;
        xSemaphoreGive(_mutex);
    }

    String json = buildJson(snap);
    _ws->textAll(json);

    // Clean up disconnected clients
    _ws->cleanupClients();
}

// ------------------------------------------------------------------------------
void ScooterWebServer::setValues(const VescValues& v) {
    if (!_vals || !_mutex) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        *_vals = v;
        xSemaphoreGive(_mutex);
    }
}

// ------------------------------------------------------------------------------
String ScooterWebServer::buildJson(const VescValues& v) {
    JsonDocument doc;
    // Assign floats directly; round to sensible precision to keep JSON compact.
    // ArduinoJson v7 serialises float with up to 7 significant digits by default;
    // we pre-round to match the display precision.
    doc["speed_kmh"]    = roundf(v.speed_kmh  * 10.0f) / 10.0f;
    doc["rpm"]          = (int)v.rpm;
    doc["duty"]         = roundf(v.duty_cycle * 1000.0f) / 10.0f;   // percent, 1 dp
    doc["v_in"]         = roundf(v.v_in       * 100.0f) / 100.0f;
    doc["battery_pct"]  = v.battery_pct;
    doc["motor_amps"]   = roundf(v.avg_motor_current * 100.0f) / 100.0f;
    doc["input_amps"]   = roundf(v.avg_input_current * 100.0f) / 100.0f;
    doc["power_w"]      = roundf(v.power_w    * 10.0f) / 10.0f;
    doc["temp_fet"]     = roundf(v.temp_mos   * 10.0f) / 10.0f;
    doc["temp_motor"]   = roundf(v.temp_motor * 10.0f) / 10.0f;
    doc["amp_hours"]    = roundf(v.amp_hours  * 1000.0f) / 1000.0f;
    doc["watt_hours"]   = roundf(v.watt_hours * 100.0f) / 100.0f;
    doc["wh_regen"]     = roundf(v.watt_hours_charged * 100.0f) / 100.0f;
    doc["fault"]        = faultString(v.fault_code);
    doc["connected"]    = v.valid;
    if (_modeIdx) doc["mode_idx"] = (int)*_modeIdx;

    String out;
    serializeJson(doc, out);
    return out;
}

// ------------------------------------------------------------------------------
void ScooterWebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                  AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_ERROR:
            Serial.printf("[WS] Error #%u: %s\n", client->id(), (char*)data);
            break;
        case WS_EVT_DATA:
            // Accept simple text commands: "ping"
            if (len == 4 && strncmp((char*)data, "ping", 4) == 0) {
                client->text("{\"pong\":true}");
            }
            break;
        default:
            break;
    }
}
