#pragma once
#include <Arduino.h>
#include "vesc_types.h"

// ==============================================================================
// VescUart — VESC packet protocol driver
//
// Packet format (VESC firmware packet.c):
//   Short  (payload ≤ 255 B): [0x02][LEN][PAYLOAD...][CRC_H][CRC_L][0x03]
//   Long   (payload > 255 B): [0x03][LEN_H][LEN_L][PAYLOAD...][CRC_H][CRC_L][0x03]
//
// CRC16-CCITT (polynomial 0x1021, init 0x0000)
// ==============================================================================

class VescUart {
public:
    // Initialise UART (call once in setup)
    void begin(int tx_pin, int rx_pin, uint32_t baud);

    // Poll VESC for full telemetry — returns true on success
    bool getValues(VescValues& out);

    // Poll firmware version — returns true on success
    bool getFwVersion(VescFwVersion& out);

    // Control commands — all return true if packet was sent (no ACK)
    bool setDuty(float duty);              // -1.0 … 1.0
    bool setCurrent(float amps);           // positive = drive, negative = regen
    bool setCurrentBrake(float amps);      // brake/regen current (positive value)
    bool setRpm(int32_t erpm);             // target ERPM
    bool sendAlive();                      // heartbeat (prevents UART timeout)

    // Last communication timestamp (ms, from millis())
    uint32_t lastRxMs() const { return _last_rx_ms; }

    // True if we got a valid response within VESC_TIMEOUT_MS × 3
    bool isConnected() const;

private:
    HardwareSerial* _serial = nullptr;

    uint32_t _last_rx_ms = 0;

    // Max VESC frame: 1 start + 3 len + 255 payload + 2 CRC + 1 end = 262 bytes
    static constexpr uint16_t VESC_MAX_FRAME = 264;

    // Send a pre-built payload; wraps in packet framing + CRC
    bool sendPayload(const uint8_t* payload, uint16_t len);

    // Wait for and decode an incoming VESC packet.
    // Returns payload length or 0 on error/timeout.
    uint16_t receivePacket(uint8_t* buf, uint16_t buf_size);

    static uint16_t crc16(const uint8_t* buf, uint16_t len);

    // Big-endian helpers
    static float    bufGetFloat32(const uint8_t* buf, float scale, int* idx);
    static float    bufGetFloat16(const uint8_t* buf, float scale, int* idx);
    static int32_t  bufGetInt32(const uint8_t* buf, int* idx);
    static int16_t  bufGetInt16(const uint8_t* buf, int* idx);

    static void     bufAppendFloat32(uint8_t* buf, float val, float scale, int* idx);
    static void     bufAppendInt32(uint8_t* buf, int32_t val, int* idx);
};
