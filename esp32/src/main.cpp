#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "config.h"
#include "vesc/vesc_uart.h"
#include "display/display_manager.h"
#include "display/ui/ui_manager.h"
#include "wifi/wifi_manager.h"
#include "web/web_server.h"
#include "ble/ble_server.h"

// ==============================================================================
// Riding mode table (referenced by config.h and UI)
// ==============================================================================
const RideMode RIDE_MODES[] = {
    { "ECO",   8.0f,  6.0f,  7000,  0x00c48c },
    { "SPORT", 20.0f, 15.0f, 11000, 0x00aaff },
    { "TURBO", 30.0f, 20.0f, 13000, 0xff4444 },
    // Uncapped speed?
};
const int RIDE_MODE_COUNT = 3;

// ==============================================================================
// Globals
// ==============================================================================
static VescUart         gVesc;
static WiFiManager      gWifi;
static ScooterWebServer gWeb;
static BleServer        gBle;

// Shared telemetry — protected by mutex
static VescValues       gValues;
static SemaphoreHandle_t gValuesMutex;

// Current ride mode index (atomic write from UI task / BLE task)
static volatile int gModeIdx = 1;   // default: SPORT

// FreeRTOS task handles
static TaskHandle_t hVescTask    = nullptr;
static TaskHandle_t hDisplayTask = nullptr;
static TaskHandle_t hAliveTask   = nullptr;

// ==============================================================================
// Task: VESC polling (Core 0)
// Polls COMM_GET_VALUES every VESC_POLL_MS.
// ==============================================================================
static void vescPollTask(void*) {
    VescValues local;
    int        last_applied_mode = -1;  // track when mode changes

    while (true) {
        const int mode = gModeIdx;
        const RideMode& m = RIDE_MODES[mode];

        // -----------------------------------------------------------------------
        // ERPM speed governor: if speed exceeds mode's max_erpm, apply gentle
        // brake current to soft-limit top speed without hard cutoff.
        // This only works when the ESP32 is sending UART commands (UART app mode).
        // -----------------------------------------------------------------------
        if (local.valid && local.rpm > m.max_erpm) {
            // Scale brake proportionally to overspeed (0–5 A per 1000 ERPM over)
            float over    = (float)(local.rpm - m.max_erpm);
            float brake_a = constrain(over / 1000.0f * 2.5f, 0.5f, 6.0f);
            gVesc.setCurrentBrake(brake_a);
        }

        // -----------------------------------------------------------------------
        // Poll telemetry
        // -----------------------------------------------------------------------
        if (gVesc.getValues(local)) {
            local.speed_kmh  = ERPM_TO_KMH(abs(local.rpm));
            local.power_w    = local.v_in * local.avg_input_current;
            int rawPct = (int)(((local.v_in - BATTERY_VOLTAGE_MIN) /
                               (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN)) * 100.0f);
            local.battery_pct = constrain(rawPct, 0, 100);

            if (xSemaphoreTake(gValuesMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gValues = local;
                xSemaphoreGive(gValuesMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(VESC_POLL_MS));
    }
}

// ==============================================================================
// Task: VESC alive heartbeat (Core 0)
// Prevents the VESC's UART timeout from firing.
// ==============================================================================
static void vescAliveTask(void*) {
    while (true) {
        gVesc.sendAlive();
        vTaskDelay(pdMS_TO_TICKS(VESC_ALIVE_MS));
    }
}

// ==============================================================================
// Task: Display + LVGL (Core 1 — same core as loop())
// LVGL is NOT thread-safe: all lv_* calls must come from the same task/core.
// We run this as a high-priority task on Core 1 alongside loop().
// loop() is kept minimal (just web tick + BLE update).
// ==============================================================================
static void displayTask(void*) {
    int lastModeIdx = -1;   // detect mode changes from BLE/web

    while (true) {
        // Snapshot the latest values (mutex-protected)
        VescValues snap;
        if (xSemaphoreTake(gValuesMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            snap = gValues;
            xSemaphoreGive(gValuesMutex);
        }

        // Propagate mode changes (from BLE callback / web) to the UI
        const int curMode = gModeIdx;
        if (curMode != lastModeIdx) {
            lastModeIdx = curMode;
            UIManager::instance().setMode(curMode);
        }

        // Update LVGL widgets with fresh data
        UIManager::instance().update(snap);

        // Drive LVGL rendering engine
        DisplayManager::instance().update();

        vTaskDelay(pdMS_TO_TICKS(5));   // ~200 fps ceiling — LVGL self-throttles
    }
}

// ==============================================================================
// setup()
// ==============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  scooter-gose-vrooom  v1.0");
    Serial.println("  ESP32-S3 + VESC MINI 6.7");
    Serial.println("========================================\n");

    // --- Shared resources ---------------------------------------------------
    gValuesMutex = xSemaphoreCreateMutex();
    configASSERT(gValuesMutex);

    // --- VESC UART ----------------------------------------------------------
    gVesc.begin(VESC_TX, VESC_RX, VESC_BAUD);

    // Fetch firmware version (non-blocking attempt)
    VescFwVersion fw;
    if (gVesc.getFwVersion(fw)) {
        Serial.printf("[VESC] FW %d.%d  HW: %s\n", fw.major, fw.minor, fw.hw_name);
    } else {
        Serial.println("[VESC] Could not read FW version — check wiring");
    }

    // --- Display + LVGL -----------------------------------------------------
    DisplayManager::instance().begin();
    UIManager::instance().begin();

    // --- WiFi AP ------------------------------------------------------------
    gWifi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, WIFI_MAX_CLIENTS);

    // --- Web server ---------------------------------------------------------
    gWeb.setModeIndexPtr(&gModeIdx);
    gWeb.setModeCallback([](int idx) {
        gModeIdx = constrain(idx, 0, RIDE_MODE_COUNT - 1);
        Serial.printf("[MODE] Web -> %s\n", RIDE_MODES[gModeIdx].name);
    });
    gWeb.begin(WEB_PORT, &gValues, gValuesMutex);

    // --- BLE ----------------------------------------------------------------
    gBle.begin(BLE_DEVICE_NAME);
    // Wire BLE NUS mode commands to the shared gModeIdx
    gBle.setModeCallback([](int idx) {
        gModeIdx = constrain(idx, 0, RIDE_MODE_COUNT - 1);
        Serial.printf("[MODE] BLE -> %s\n", RIDE_MODES[gModeIdx].name);
    });

    // --- Update settings screen with network info ---------------------------
    UIManager::instance().setNetworkInfo(
        WIFI_SSID, gWifi.ip().c_str(), BLE_DEVICE_NAME);
    if (fw.valid) {
        UIManager::instance().setVescFwVersion(fw.major, fw.minor, fw.hw_name);
    }

    // --- FreeRTOS tasks -----------------------------------------------------
    // VESC poll on Core 0
    xTaskCreatePinnedToCore(vescPollTask, "vesc_poll", 4096, nullptr, 2,
                            &hVescTask, 0);

    // VESC alive heartbeat on Core 0
    xTaskCreatePinnedToCore(vescAliveTask, "vesc_alive", 2048, nullptr, 1,
                            &hAliveTask, 0);

    // Display task on Core 1 (same core as loop, but we give it priority)
    xTaskCreatePinnedToCore(displayTask, "display", 8192, nullptr, 3,
                            &hDisplayTask, 1);

    Serial.println("[BOOT] All tasks started");
    Serial.printf("[BOOT] Free heap: %u bytes  PSRAM: %u bytes\n",
                  esp_get_free_heap_size(), esp_get_free_internal_heap_size());
}

// ==============================================================================
// loop() — runs on Core 1 at low priority
// Handles web server tick and BLE updates.
// Display + LVGL are handled by the dedicated displayTask above.
// ==============================================================================
void loop() {
    // --- Web server: push WebSocket telemetry --------------------------------
    gWeb.tick();

    // --- BLE telemetry -------------------------------------------------------
    VescValues snap;
    if (xSemaphoreTake(gValuesMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        snap = gValues;
        xSemaphoreGive(gValuesMutex);
    }
    gBle.update(snap);

    // --- Watchdog / diagnostics every 10 s -----------------------------------
    static uint32_t last_diag = 0;
    if (millis() - last_diag > 10000) {
        last_diag = millis();
        Serial.printf("[DIAG] Speed=%.1f km/h  Bat=%d%%  V=%.2f  T=%.1f°C  "
                      "Connected=%d  WiFi clients=%d  BLE=%d\n",
                      snap.speed_kmh, snap.battery_pct, snap.v_in,
                      snap.temp_mos, snap.valid,
                      gWifi.clientCount(), gBle.isConnected());
    }

    delay(50);  // yield — display task has higher priority
}
