#include "ble_server.h"
#include "config.h"
#include <BLEAdvertising.h>

// ==============================================================================
void BleServer::begin(const char* device_name) {
    BLEDevice::init(device_name);
    _server = BLEDevice::createServer();
    _server->setCallbacks(this);

    // --- Battery Service (standard) ------------------------------------------
    BLEService* svc_bat = _server->createService(BLE_SVC_BATTERY);
    _chr_bat = svc_bat->createCharacteristic(
                   BLE_CHR_BATTERY_LEVEL,
                   BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    _chr_bat->addDescriptor(new BLE2902());
    svc_bat->start();

    // --- Scooter Telemetry Service (custom) -----------------------------------
    BLEService* svc_sc = _server->createService(BLE_SVC_SCOOTER);

    auto mkChr = [&](const char* uuid) {
        return svc_sc->createCharacteristic(
            uuid,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    };

    _chr_speed = mkChr(BLE_CHR_SPEED);   _chr_speed->addDescriptor(new BLE2902());
    _chr_volt  = mkChr(BLE_CHR_VOLTAGE); _chr_volt->addDescriptor(new BLE2902());
    _chr_temp  = mkChr(BLE_CHR_TEMP);    _chr_temp->addDescriptor(new BLE2902());
    _chr_curr  = mkChr(BLE_CHR_CURRENT); _chr_curr->addDescriptor(new BLE2902());

    // Set initial values
    _chr_speed->setValue("0.0");
    _chr_volt->setValue("0.0");
    _chr_temp->setValue("0.0");
    _chr_curr->setValue("0.0");

    svc_sc->start();

    // --- Nordic UART Service -------------------------------------------------
    BLEService* svc_nus = _server->createService(BLE_SVC_NUS);

    _chr_nus_tx = svc_nus->createCharacteristic(
                      BLE_CHR_NUS_TX,
                      BLECharacteristic::PROPERTY_NOTIFY);
    _chr_nus_tx->addDescriptor(new BLE2902());

    _chr_nus_rx = svc_nus->createCharacteristic(
                      BLE_CHR_NUS_RX,
                      BLECharacteristic::PROPERTY_WRITE);
    _chr_nus_rx->setCallbacks(&_nusRxCb);

    svc_nus->start();

    // --- Advertising ---------------------------------------------------------
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_BATTERY);
    adv->addServiceUUID(BLE_SVC_NUS);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);  // iPhone compatibility
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] Advertising as \"%s\"\n", device_name);
}

// ------------------------------------------------------------------------------
void BleServer::update(const VescValues& v) {
    uint32_t now = millis();
    if (now - _last_update_ms < UPDATE_INTERVAL_MS) return;
    _last_update_ms = now;

    // Battery level (uint8)
    uint8_t bat = (uint8_t)constrain(v.battery_pct, 0, 100);
    _chr_bat->setValue(&bat, 1);

    // Telemetry as ASCII strings (easy to read in any BLE app)
    char buf[16];

    snprintf(buf, sizeof(buf), "%.1f", v.speed_kmh);
    _chr_speed->setValue(buf);

    snprintf(buf, sizeof(buf), "%.2f", v.v_in);
    _chr_volt->setValue(buf);

    snprintf(buf, sizeof(buf), "%.1f", v.temp_mos);
    _chr_temp->setValue(buf);

    snprintf(buf, sizeof(buf), "%.2f", v.avg_input_current);
    _chr_curr->setValue(buf);

    if (_connected) {
        _chr_bat->notify();
        _chr_speed->notify();
        _chr_volt->notify();
        _chr_temp->notify();
        _chr_curr->notify();

        // NUS TX: compact CSV telemetry line
        snprintf(buf, sizeof(buf), "%.1f,%.1f,%d", v.speed_kmh, v.v_in, v.battery_pct);
        _chr_nus_tx->setValue(buf);
        _chr_nus_tx->notify();
    }
}

// ------------------------------------------------------------------------------
void BleServer::onConnect(BLEServer*) {
    _connected = true;
    Serial.println("[BLE] Client connected");
}

void BleServer::onDisconnect(BLEServer* srv) {
    _connected = false;
    Serial.println("[BLE] Client disconnected — restarting advertising");
    BLEDevice::startAdvertising();
}

// ------------------------------------------------------------------------------
void BleServer::NusRxCallbacks::onWrite(BLECharacteristic* chr) {
    // getValue() returns std::string — convert to Arduino String for easy compare
    String val = chr->getValue().c_str();
    val.trim();
    if (val.isEmpty()) return;

    Serial.printf("[BLE/NUS RX] \"%s\"\n", val.c_str());

    // Mode commands — fire callback if registered
    int newMode = -1;
    if (val.equalsIgnoreCase("mode:eco"))   newMode = 0;
    else if (val.equalsIgnoreCase("mode:sport")) newMode = 1;
    else if (val.equalsIgnoreCase("mode:turbo")) newMode = 2;
    else if (val.equalsIgnoreCase("ping"))   { chr->setValue("pong
"); return; }

    if (newMode >= 0 && _parent->_modeCallback) {
        _parent->_modeCallback(newMode);
        // Acknowledge via NUS TX
        char ack[24];
        snprintf(ack, sizeof(ack), "mode:%d\n", newMode);
        if (_parent->_chr_nus_tx) {
            _parent->_chr_nus_tx->setValue(ack);
            _parent->_chr_nus_tx->notify();
        }
    }
}
