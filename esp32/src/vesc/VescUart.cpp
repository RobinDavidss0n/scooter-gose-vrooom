#include "vesc/VescUart.h"

#include <math.h>
#include <string.h>

namespace scooter {

namespace {

constexpr uint8_t kShortFrameStart = 2;
constexpr uint8_t kLongFrameStart = 3;
constexpr uint8_t kFrameStop = 3;

}  // namespace

VescUart::VescUart(HardwareSerial &serial, int rxPin, int txPin, uint32_t baudRate)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baudRate(baudRate) {}

void VescUart::begin() {
    _serial.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
}

void VescUart::poll(uint32_t nowMs) {
    while (_serial.available() > 0) {
        const int raw = _serial.read();
        if (raw < 0) {
            break;
        }

        if (_rxLength >= kRxBufferSize) {
            discardPrefix(1);
        }

        _rxBuffer[_rxLength++] = static_cast<uint8_t>(raw);
    }

    processFrames(nowMs);
}

void VescUart::requestTelemetry() {
    const uint8_t payload[] = {static_cast<uint8_t>(VescCommandId::GetValues)};
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendAlive() {
    const uint8_t payload[] = {static_cast<uint8_t>(VescCommandId::Alive)};
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendCurrent(float currentA) {
    const int32_t scaled = static_cast<int32_t>(lroundf(currentA * 1000.0f));
    uint8_t payload[5] = {static_cast<uint8_t>(VescCommandId::SetCurrent), 0, 0, 0, 0};
    payload[1] = static_cast<uint8_t>((scaled >> 24) & 0xFF);
    payload[2] = static_cast<uint8_t>((scaled >> 16) & 0xFF);
    payload[3] = static_cast<uint8_t>((scaled >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(scaled & 0xFF);
    sendPayload(payload, sizeof(payload));
}

void VescUart::sendBrakeCurrent(float currentA) {
    const float clamped = currentA < 0.0f ? 0.0f : currentA;
    const int32_t scaled = static_cast<int32_t>(lroundf(clamped * 1000.0f));
    uint8_t payload[5] = {static_cast<uint8_t>(VescCommandId::SetCurrentBrake), 0, 0, 0, 0};
    payload[1] = static_cast<uint8_t>((scaled >> 24) & 0xFF);
    payload[2] = static_cast<uint8_t>((scaled >> 16) & 0xFF);
    payload[3] = static_cast<uint8_t>((scaled >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(scaled & 0xFF);
    sendPayload(payload, sizeof(payload));
}

bool VescUart::isConnected(uint32_t nowMs, uint32_t timeoutMs) const {
    return _lastRxMs != 0 && (nowMs - _lastRxMs) <= timeoutMs;
}

bool VescUart::hasFreshTelemetry(uint32_t nowMs, uint32_t maxAgeMs) const {
    return _telemetry.valid && (nowMs - _telemetry.lastResponseMs) <= maxAgeMs;
}

void VescUart::processFrames(uint32_t nowMs) {
    while (_rxLength > 0) {
        const uint8_t start = _rxBuffer[0];

        size_t headerLength = 0;
        size_t payloadLength = 0;
        size_t frameLength = 0;

        if (start == kShortFrameStart) {
            if (_rxLength < 5) {
                return;
            }
            headerLength = 2;
            payloadLength = _rxBuffer[1];
            frameLength = headerLength + payloadLength + 3;
        } else if (start == kLongFrameStart) {
            if (_rxLength < 6) {
                return;
            }
            headerLength = 3;
            payloadLength = (static_cast<size_t>(_rxBuffer[1]) << 8) | _rxBuffer[2];
            frameLength = headerLength + payloadLength + 3;
        } else {
            discardPrefix(1);
            continue;
        }

        if (frameLength > kRxBufferSize) {
            discardPrefix(1);
            continue;
        }

        if (_rxLength < frameLength) {
            return;
        }

        if (_rxBuffer[frameLength - 1] != kFrameStop) {
            discardPrefix(1);
            continue;
        }

        const size_t crcIndex = headerLength + payloadLength;
        const uint16_t expectedCrc = (static_cast<uint16_t>(_rxBuffer[crcIndex]) << 8) |
                                     static_cast<uint16_t>(_rxBuffer[crcIndex + 1]);
        const uint16_t actualCrc = crc16(&_rxBuffer[headerLength], payloadLength);
        if (expectedCrc != actualCrc) {
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
        return;
    }

    decoded.valid = true;
    decoded.lastResponseMs = nowMs;
    _telemetry = decoded;
}

bool VescUart::decodeGetValues(const uint8_t *payload, size_t length, VescTelemetry &out) const {
    if (length < 58 || payload[0] != static_cast<uint8_t>(VescCommandId::GetValues)) {
        return false;
    }

    size_t offset = 1;
    out.mosfetTempC = static_cast<float>(readInt16(payload, offset)) / 10.0f;
    out.motorTempC = static_cast<float>(readInt16(payload, offset)) / 10.0f;
    out.motorCurrentA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.inputCurrentA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.currentIdA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.currentIqA = static_cast<float>(readInt32(payload, offset)) / 100.0f;
    out.dutyCycle = static_cast<float>(readInt16(payload, offset)) / 1000.0f;
    out.rpm = readInt32(payload, offset);
    out.inputVoltageV = static_cast<float>(readInt16(payload, offset)) / 10.0f;
    out.ampHours = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.ampHoursCharged = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.wattHours = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.wattHoursCharged = static_cast<float>(readInt32(payload, offset)) / 10000.0f;
    out.tachometer = readInt32(payload, offset);
    out.tachometerAbs = readInt32(payload, offset);
    out.faultCode = payload[offset++];
    out.pidPosition = static_cast<float>(readInt32(payload, offset)) / 1000000.0f;
    out.controllerId = (offset < length) ? payload[offset] : 0;
    return true;
}

void VescUart::sendPayload(const uint8_t *payload, size_t length) {
    if (length == 0 || length > 0xFFFF) {
        return;
    }

    uint16_t crc = crc16(payload, length);

    if (length <= 0xFF) {
        uint8_t frame[kRxBufferSize] = {0};
        const size_t frameLength = length + 5;
        frame[0] = kShortFrameStart;
        frame[1] = static_cast<uint8_t>(length);
        memcpy(frame + 2, payload, length);
        frame[2 + length] = static_cast<uint8_t>((crc >> 8) & 0xFF);
        frame[3 + length] = static_cast<uint8_t>(crc & 0xFF);
        frame[4 + length] = kFrameStop;
        _serial.write(frame, frameLength);
    } else {
        uint8_t frame[kRxBufferSize] = {0};
        const size_t frameLength = length + 6;
        frame[0] = kLongFrameStart;
        frame[1] = static_cast<uint8_t>((length >> 8) & 0xFF);
        frame[2] = static_cast<uint8_t>(length & 0xFF);
        memcpy(frame + 3, payload, length);
        frame[3 + length] = static_cast<uint8_t>((crc >> 8) & 0xFF);
        frame[4 + length] = static_cast<uint8_t>(crc & 0xFF);
        frame[5 + length] = kFrameStop;
        _serial.write(frame, frameLength);
    }

    _lastTxMs = millis();
}

uint16_t VescUart::crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0;
    for (size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint16_t>(data[index]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

int16_t VescUart::readInt16(const uint8_t *data, size_t &offset) {
    const int16_t value = static_cast<int16_t>((static_cast<uint16_t>(data[offset]) << 8) |
                                               static_cast<uint16_t>(data[offset + 1]));
    offset += 2;
    return value;
}

int32_t VescUart::readInt32(const uint8_t *data, size_t &offset) {
    const int32_t value = (static_cast<int32_t>(data[offset]) << 24) |
                          (static_cast<int32_t>(data[offset + 1]) << 16) |
                          (static_cast<int32_t>(data[offset + 2]) << 8) |
                          static_cast<int32_t>(data[offset + 3]);
    offset += 4;
    return value;
}

}  // namespace scooter