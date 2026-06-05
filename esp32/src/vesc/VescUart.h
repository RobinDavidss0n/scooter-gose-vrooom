#pragma once

#include <Arduino.h>

#include "vesc/VescTypes.h"

namespace scooter {

class VescUart {
public:
    VescUart(HardwareSerial &serial, int rxPin, int txPin, uint32_t baudRate);

    // --- Lifecycle ---
    void begin();
    void poll(uint32_t nowMs);

    // --- Commands ---
    void requestTelemetry();
    void sendAlive();
    void sendCurrent(float currentA);
    void sendBrakeCurrent(float currentA);

    // --- State ---
    bool isConnected(uint32_t nowMs, uint32_t timeoutMs = 1000) const;
    bool hasFreshTelemetry(uint32_t nowMs, uint32_t maxAgeMs = 500) const;

    const VescTelemetry &telemetry() const { return _telemetry; }
    uint32_t             lastRxMs()  const { return _lastRxMs;  }
    uint32_t             lastTxMs()  const { return _lastTxMs;  }

private:
    static constexpr size_t kRxBufferSize = 512;

    HardwareSerial &_serial;
    const int       _rxPin;
    const int       _txPin;
    const uint32_t  _baudRate;

    uint8_t _rxBuffer[kRxBufferSize] = {0};
    size_t  _rxLength                = 0;

    VescTelemetry _telemetry;
    uint32_t      _lastRxMs = 0;
    uint32_t      _lastTxMs = 0;

    // --- RX processing ---
    void processFrames(uint32_t nowMs);
    void discardPrefix(size_t count);
    void handlePayload(const uint8_t *payload, size_t length, uint32_t nowMs);
    bool decodeGetValues(const uint8_t *payload, size_t length, VescTelemetry &out) const;

    // --- TX helpers ---
    void sendInt32Cmd(VescCommandId cmd, int32_t value);
    void sendPayload(const uint8_t *payload, size_t length);

    // --- Codec utilities ---
    static uint16_t crc16(const uint8_t *data, size_t length);
    static int16_t  readInt16(const uint8_t *data, size_t &offset);
    static int32_t  readInt32(const uint8_t *data, size_t &offset);
};

}  // namespace scooter