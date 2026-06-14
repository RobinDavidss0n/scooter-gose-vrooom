#include "vesc/VescUart.h"

#include <math.h>
#include <string.h>

// Uncomment to enable human-readable VESC UART diagnostics.
#define VESC_UART_DEBUG
// Print one debug line every N successfully decoded telemetry frames.
static constexpr uint32_t kDebugPrintInterval = 25;

namespace scooter {

// ---------------------------------------------------------------------------
// Frame framing constants (VESC serial protocol).
// NOTE: kFrameStop == kLongFrameStart (both 3) is intentional per spec.
// ---------------------------------------------------------------------------

namespace {

constexpr uint8_t kShortFrameStart = 2;  // followed by 1-byte payload length
constexpr uint8_t kLongFrameStart  = 3;  // followed by 2-byte payload length
constexpr uint8_t kFrameStop       = 3;  // terminates every frame

}  // namespace

// ---------------------------------------------------------------------------
// Construction / initialisation
// ---------------------------------------------------------------------------

VescUart::VescUart(HardwareSerial &serial, int rxPin, int txPin, uint32_t baudRate)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baudRate(baudRate) {}

void VescUart::begin() {
    _serial.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
}

// ---------------------------------------------------------------------------
// Main loop entry point
// ---------------------------------------------------------------------------

void VescUart::poll(uint32_t nowMs) {

    while (_serial.available() > 0) {
        
        const int raw = _serial.read();
        if (raw < 0) {
            break;
        }

        if (_rxLength >= kRxBufferSize) {
            #ifdef VESC_UART_DEBUG
            Serial.println("[VESC] RX buffer full — discarding oldest byte");
            #endif
            discardPrefix(1);
        }

        _rxBuffer[_rxLength++] = static_cast<uint8_t>(raw);
    }

    processFrames(nowMs);
}

// ---------------------------------------------------------------------------
// Outgoing commands
// ---------------------------------------------------------------------------

void VescUart::requestTelemetry() {
    const uint8_t payload[] = {static_cast<uint8_t>(VescCommandId::GetValues)};
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendAlive() {
    const uint8_t payload[] = {static_cast<uint8_t>(VescCommandId::Alive)};
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendCurrent(float currentA) {
    sendInt32Cmd(VescCommandId::SetCurrent,
                 static_cast<int32_t>(lroundf(currentA * 1000.0f)));
}

void VescUart::sendBrakeCurrent(float currentA) {
    const float clamped = currentA < 0.0f ? 0.0f : currentA;
    sendInt32Cmd(VescCommandId::SetCurrentBrake,
                 static_cast<int32_t>(lroundf(clamped * 1000.0f)));
}

// ---------------------------------------------------------------------------
// Connection / telemetry status
// ---------------------------------------------------------------------------

bool VescUart::isConnected(uint32_t nowMs, uint32_t timeoutMs) const {
    return _lastRxMs != 0 && (nowMs - _lastRxMs) <= timeoutMs;
}

bool VescUart::hasFreshTelemetry(uint32_t nowMs, uint32_t maxAgeMs) const {
    return _telemetry.valid && (nowMs - _telemetry.lastResponseMs) <= maxAgeMs;
}

bool VescUart::hasRecentRxError(uint32_t nowMs, uint32_t windowMs) const {
    return _lastRxErrorMs != 0 && (nowMs - _lastRxErrorMs) <= windowMs;
}

// ---------------------------------------------------------------------------
// RX: frame parsing
// ---------------------------------------------------------------------------

void VescUart::processFrames(uint32_t nowMs) {
    while (_rxLength > 0) {
        const uint8_t start = _rxBuffer[0];

        size_t headerLength  = 0;
        size_t payloadLength = 0;

        if (start == kShortFrameStart) {
            if (_rxLength < 5) {
                return;  // wait for more bytes
            }
            headerLength  = 2;
            payloadLength = _rxBuffer[1];
        } else if (start == kLongFrameStart) {
            if (_rxLength < 6) {
                return;  // wait for more bytes
            }
            headerLength  = 3;
            payloadLength = (static_cast<size_t>(_rxBuffer[1]) << 8) | _rxBuffer[2];
        } else {
            #ifdef VESC_UART_DEBUG
            Serial.printf("[VESC] Bad start byte 0x%02X — discarding\n", start);
            #endif
            _lastRxErrorMs = nowMs;
            discardPrefix(1);
            continue;
        }

        const size_t frameLength = headerLength + payloadLength + 3;  // +crc(2) +stop(1)

        if (frameLength > kRxBufferSize) {
            #ifdef VESC_UART_DEBUG
            Serial.printf("[VESC] Frame too large (%u) — discarding\n", (unsigned)frameLength);
            #endif
            _lastRxErrorMs = nowMs;
            discardPrefix(1);
            continue;
        }

        if (_rxLength < frameLength) {
            return;  // wait for more bytes
        }

        if (_rxBuffer[frameLength - 1] != kFrameStop) {
            #ifdef VESC_UART_DEBUG
            Serial.printf("[VESC] Bad stop byte 0x%02X — discarding\n",
                          _rxBuffer[frameLength - 1]);
            #endif
            _lastRxErrorMs = nowMs;
            discardPrefix(1);
            continue;
        }

        const size_t   crcIndex    = headerLength + payloadLength;
        const uint16_t expectedCrc = (static_cast<uint16_t>(_rxBuffer[crcIndex]) << 8) |
                                      static_cast<uint16_t>(_rxBuffer[crcIndex + 1]);
        const uint16_t actualCrc   = crc16(&_rxBuffer[headerLength], payloadLength);

        if (expectedCrc != actualCrc) {
            #ifdef VESC_UART_DEBUG
            Serial.printf("[VESC] CRC mismatch: expected=0x%04X actual=0x%04X — discarding\n",
                          expectedCrc, actualCrc);
            #endif
            _lastRxErrorMs = nowMs;
            discardPrefix(1);
            continue;
        }

        handlePayload(&_rxBuffer[headerLength], payloadLength, nowMs);
        discardPrefix(frameLength);
    }
}

void VescUart::discardPrefix(size_t count) {
    if (count >= _rxLength) {
        _rxLength = 0;
        return;
    }

    memmove(_rxBuffer, _rxBuffer + count, _rxLength - count);
    _rxLength -= count;
}

void VescUart::handlePayload(const uint8_t *payload, size_t length, uint32_t nowMs) {
    if (length == 0) {
        return;
    }

    _lastRxMs = nowMs;

    if (payload[0] != static_cast<uint8_t>(VescCommandId::GetValues)) {
        return;
    }

    VescTelemetry decoded;
    if (!decodeGetValues(payload, length, decoded)) {
        #ifdef VESC_UART_DEBUG
        Serial.printf("[VESC] decodeGetValues failed (%u bytes, need >=58)\n",
                      (unsigned)length);
        #endif
        _lastRxErrorMs = nowMs;
        return;
    }

    decoded.valid          = true;
    decoded.lastResponseMs = nowMs;
    _telemetry             = decoded;

    #ifdef VESC_UART_DEBUG
    static uint32_t debugCount = 0;
    if (++debugCount % kDebugPrintInterval == 0) {
        Serial.printf("[VESC] #%lu  RPM=%ld  Vin=%.1fV  motorA=%.2f  inputA=%.2f  fault=%u\n",
                      (unsigned long)debugCount,
                      (long)decoded.rpm,
                      decoded.inputVoltageV,
                      decoded.motorCurrentA,
                      decoded.inputCurrentA,
                      decoded.faultCode);
    }
    #endif
}

// ---------------------------------------------------------------------------
// RX: telemetry decoding
// Minimum payload: 1 (cmd) + 57 (fields through pidPosition) = 58 bytes.
// controllerId at byte 58 is optional (older firmware may omit it).
// ---------------------------------------------------------------------------

bool VescUart::decodeGetValues(const uint8_t *payload, size_t length, VescTelemetry &out) const {
    if (length < 58 || payload[0] != static_cast<uint8_t>(VescCommandId::GetValues)) {
        return false;
    }

    size_t offset = 1;

    out.mosfetTempC = static_cast<float>(readInt16(payload, offset)) / 10.0f;
    out.motorTempC  = static_cast<float>(readInt16(payload, offset)) / 10.0f;

    out.motorCurrentA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.inputCurrentA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.currentIdA    = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.currentIqA    = static_cast<float>(readInt32(payload, offset)) / 100.0f;

    out.dutyCycle     = static_cast<float>(readInt16(payload, offset)) / 1000.0f;
    out.rpm           = readInt32(payload, offset);
    out.inputVoltageV = static_cast<float>(readInt16(payload, offset)) / 10.0f;

    out.ampHours         = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.ampHoursCharged  = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.wattHours        = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.wattHoursCharged = static_cast<float>(readInt32(payload, offset)) / 10000.0f;

    out.tachometer    = readInt32(payload, offset);
    out.tachometerAbs = readInt32(payload, offset);

    out.faultCode   = payload[offset++];
    out.pidPosition = static_cast<float>(readInt32(payload, offset)) / 1000000.0f;
    out.controllerId = (offset < length) ? payload[offset] : 0;

    return true;
}

// ---------------------------------------------------------------------------
// TX: frame assembly and transmission
// ---------------------------------------------------------------------------

void VescUart::sendInt32Cmd(VescCommandId cmd, int32_t value) {
    uint8_t payload[5];
    payload[0] = static_cast<uint8_t>(cmd);
    payload[1] = static_cast<uint8_t>((value >> 24) & 0xFF);
    payload[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    payload[3] = static_cast<uint8_t>((value >> 8)  & 0xFF);
    payload[4] = static_cast<uint8_t>(value          & 0xFF);
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendPayload(const uint8_t *payload, size_t length) {
    if (length == 0 || length > 0xFFFF) {
        return;
    }

    const uint16_t crc = crc16(payload, length);

    uint8_t frame[kRxBufferSize];
    size_t  fi = 0;

    // Header: start byte + encoded payload length
    if (length <= 0xFF) {
        frame[fi++] = kShortFrameStart;
        frame[fi++] = static_cast<uint8_t>(length);
    } else {
        frame[fi++] = kLongFrameStart;
        frame[fi++] = static_cast<uint8_t>((length >> 8) & 0xFF);
        frame[fi++] = static_cast<uint8_t>(length & 0xFF);
    }

    // Payload
    memcpy(frame + fi, payload, length);
    fi += length;

    // Trailer: CRC + stop byte
    frame[fi++] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    frame[fi++] = static_cast<uint8_t>(crc & 0xFF);
    frame[fi++] = kFrameStop;

    _serial.write(frame, fi);
    _lastTxMs = millis();
}

// ---------------------------------------------------------------------------
// Codec utilities
// ---------------------------------------------------------------------------

uint16_t VescUart::crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0;
    for (size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint16_t>(data[index]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                                  : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

int16_t VescUart::readInt16(const uint8_t *data, size_t &offset) {
    const int16_t value = static_cast<int16_t>(
        (static_cast<uint16_t>(data[offset]) << 8) |
         static_cast<uint16_t>(data[offset + 1]));
    offset += 2;
    return value;
}

int32_t VescUart::readInt32(const uint8_t *data, size_t &offset) {
    const int32_t value = (static_cast<int32_t>(data[offset])     << 24) |
                          (static_cast<int32_t>(data[offset + 1]) << 16) |
                          (static_cast<int32_t>(data[offset + 2]) <<  8) |
                           static_cast<int32_t>(data[offset + 3]);
    offset += 4;
    return value;
}

}  // namespace scooter
