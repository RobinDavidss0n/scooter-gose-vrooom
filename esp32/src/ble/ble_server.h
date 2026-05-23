#pragma once
#include <Arduino.h>
#include <functional>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "vesc/vesc_types.h"

// ==============================================================================
// BleServer — BLE GATT server for the scooter
//
// Services:
//
//  1. Battery Service (standard, 0x180F)
//     - Battery Level characteristic (0x2A19): uint8, 0–100 %
//
//  2. ScooterTelemetry Service (custom UUID)
//     - Speed      characteristic: float as ASCII string "25.3"
//     - Voltage    characteristic: float as ASCII string "38.5"
//     - Temp FET   characteristic: float as ASCII string "42.1"
//     - Current    characteristic: float as ASCII string "4.2"
//
//  3. Nordic UART Service (NUS, 0x6E400001-…)
//     - TX (notify) : ESP32 → phone  (telemetry lines)
//     - RX (write)  : phone → ESP32  (commands: "mode:eco", "mode:sport", …)
//
// Advertises: "ScooterCtrl"
// ==============================================================================

// Service / characteristic UUIDs
#define BLE_SVC_BATTERY        "180F"
#define BLE_CHR_BATTERY_LEVEL  "2A19"

#define BLE_SVC_SCOOTER        "19B10000-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHR_SPEED          "19B10001-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHR_VOLTAGE        "19B10002-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHR_TEMP           "19B10003-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHR_CURRENT        "19B10004-E8F2-537E-4F6C-D104768A1214"

#define BLE_SVC_NUS            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHR_NUS_TX         "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // notify
#define BLE_CHR_NUS_RX         "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // write

class BleServer : public BLEServerCallbacks {
public:
    void begin(const char* device_name);

    // Push new telemetry to all BLE characteristics + NUS TX
    void update(const VescValues& v);

    bool isConnected() const { return _connected; }

    // Register a callback that fires when the client sends a mode command
    // via NUS RX ("mode:eco", "mode:sport", "mode:turbo").
    // The argument is the new mode index (0=ECO, 1=SPORT, 2=TURBO).
    void setModeCallback(std::function<void(int)> cb) { _modeCallback = cb; }

    // BLEServerCallbacks
    void onConnect(BLEServer* srv) override;
    void onDisconnect(BLEServer* srv) override;

private:
    BLEServer*         _server    = nullptr;
    BLECharacteristic* _chr_bat   = nullptr;
    BLECharacteristic* _chr_speed = nullptr;
    BLECharacteristic* _chr_volt  = nullptr;
    BLECharacteristic* _chr_temp  = nullptr;
    BLECharacteristic* _chr_curr  = nullptr;
    BLECharacteristic* _chr_nus_tx = nullptr;
    BLECharacteristic* _chr_nus_rx = nullptr;

    bool _connected = false;
    uint32_t _last_update_ms = 0;

    std::function<void(int)> _modeCallback;  // called when mode command received

    static constexpr uint32_t UPDATE_INTERVAL_MS = 500;

    // NUS RX write handler — holds a back-pointer so it can invoke the callback
    class NusRxCallbacks : public BLECharacteristicCallbacks {
    public:
        explicit NusRxCallbacks(BleServer* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* chr) override;
    private:
        BleServer* _parent;
    };
    NusRxCallbacks _nusRxCb{this};
};
