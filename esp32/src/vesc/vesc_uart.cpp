#include "vesc_uart.h"
#include "config.h"

// ==============================================================================
// CRC16-CCITT table (polynomial 0x1021)
// ==============================================================================
static const uint16_t CRC16_TAB[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
    0x4864,0x5845,0x6826,0x7807,0x08e0,0x18c1,0x28a2,0x3883,
    0xc96c,0xd94d,0xe92e,0xf90f,0x89e8,0x99c9,0xa9aa,0xb98b,
    0x5a55,0x4a74,0x7a17,0x6a36,0x1ad1,0x0af0,0x3a93,0x2ab2,
    0xdb5d,0xcb7c,0xfb1f,0xeb3e,0x9bd9,0x8bf8,0xbb9b,0xabba,
    0x6c66,0x7c47,0x4c24,0x5c05,0x2ce2,0x3cc3,0x0ca0,0x1c81,
    0xed6e,0xfd4f,0xcd2c,0xdd0d,0xadea,0xbdcb,0x8da8,0x9d89,
    0x7e57,0x6e76,0x5e15,0x4e34,0x3ed3,0x2ef2,0x1e91,0x0eb0,
    0xff5f,0xef7e,0xdf1d,0xcf3c,0xbffb,0xafda,0x9fb9,0x8f98,
    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

// ==============================================================================
// Public API
// ==============================================================================

void VescUart::begin(int tx_pin, int rx_pin, uint32_t baud) {
    _serial = &Serial1;
    _serial->begin(baud, SERIAL_8N1, rx_pin, tx_pin);
    _serial->setTimeout(VESC_TIMEOUT_MS);
    Serial.printf("[VESC] UART started — TX=%d RX=%d @%u baud\n", tx_pin, rx_pin, baud);
}

// ------------------------------------------------------------------------------
bool VescUart::getValues(VescValues& out) {
    const uint8_t payload[] = { COMM_GET_VALUES };
    if (!sendPayload(payload, 1)) return false;

    uint8_t buf[128];
    uint16_t len = receivePacket(buf, sizeof(buf));
    if (len < 55) return false;      // minimum expected payload size
    if (buf[0] != COMM_GET_VALUES) return false;

    int idx = 1;
    out.temp_mos           = bufGetFloat16(buf, 10.0f, &idx);
    out.temp_motor         = bufGetFloat16(buf, 10.0f, &idx);
    out.avg_motor_current  = bufGetFloat32(buf, 100.0f, &idx);
    out.avg_input_current  = bufGetFloat32(buf, 100.0f, &idx);
    out.avg_id             = bufGetFloat32(buf, 100.0f, &idx);
    out.avg_iq             = bufGetFloat32(buf, 100.0f, &idx);
    out.duty_cycle         = bufGetFloat16(buf, 1000.0f, &idx);
    out.rpm                = bufGetFloat32(buf, 1.0f, &idx);
    out.v_in               = bufGetFloat16(buf, 10.0f, &idx);
    out.amp_hours          = bufGetFloat32(buf, 10000.0f, &idx);
    out.amp_hours_charged  = bufGetFloat32(buf, 10000.0f, &idx);
    out.watt_hours         = bufGetFloat32(buf, 10000.0f, &idx);
    out.watt_hours_charged = bufGetFloat32(buf, 10000.0f, &idx);
    out.tachometer         = bufGetInt32(buf, &idx);
    out.tachometer_abs     = bufGetInt32(buf, &idx);
    out.fault_code         = static_cast<mc_fault_code>(buf[idx++]);

    // Derived fields
    out.speed_kmh  = ERPM_TO_KMH(fabsf(out.rpm));
    out.battery_pct = constrain(VOLTAGE_TO_PCT(out.v_in), 0, 100);
    out.power_w    = out.avg_input_current * out.v_in;
    out.valid      = true;

    _last_rx_ms = millis();
    return true;
}

// ------------------------------------------------------------------------------
bool VescUart::getFwVersion(VescFwVersion& out) {
    const uint8_t payload[] = { COMM_FW_VERSION };
    if (!sendPayload(payload, 1)) return false;

    uint8_t buf[64];
    uint16_t len = receivePacket(buf, sizeof(buf));
    if (len < 3) return false;
    if (buf[0] != COMM_FW_VERSION) return false;

    out.major = buf[1];
    out.minor = buf[2];
    if (len > 3) {
        uint8_t hw_len = min((int)(len - 3), (int)sizeof(out.hw_name) - 1);
        memcpy(out.hw_name, &buf[3], hw_len);
        out.hw_name[hw_len] = '\0';
    }
    out.valid = true;
    return true;
}

// ------------------------------------------------------------------------------
bool VescUart::setDuty(float duty) {
    duty = constrain(duty, -1.0f, 1.0f);
    uint8_t payload[5];
    payload[0] = COMM_SET_DUTY;
    int idx = 1;
    bufAppendFloat32(payload, duty, 100000.0f, &idx);
    return sendPayload(payload, 5);
}

bool VescUart::setCurrent(float amps) {
    uint8_t payload[5];
    payload[0] = COMM_SET_CURRENT;
    int idx = 1;
    bufAppendFloat32(payload, amps, 1000.0f, &idx);
    return sendPayload(payload, 5);
}

bool VescUart::setCurrentBrake(float amps) {
    if (amps < 0.0f) amps = -amps;   // ensure positive
    uint8_t payload[5];
    payload[0] = COMM_SET_CURRENT_BRAKE;
    int idx = 1;
    bufAppendFloat32(payload, amps, 1000.0f, &idx);
    return sendPayload(payload, 5);
}

bool VescUart::setRpm(int32_t erpm) {
    uint8_t payload[5];
    payload[0] = COMM_SET_RPM;
    int idx = 1;
    bufAppendInt32(payload, erpm, &idx);
    return sendPayload(payload, 5);
}

bool VescUart::sendAlive() {
    const uint8_t payload[] = { COMM_ALIVE };
    return sendPayload(payload, 1);
}

bool VescUart::isConnected() const {
    return (millis() - _last_rx_ms) < (uint32_t)(VESC_TIMEOUT_MS * 6);
}

// ==============================================================================
// Private — packet layer
// ==============================================================================

bool VescUart::sendPayload(const uint8_t* payload, uint16_t len) {
    if (!_serial || len > 255) return false;   // we only ever send short packets

    uint8_t frame[VESC_MAX_FRAME];
    int fi = 0;

    frame[fi++] = 0x02;
    frame[fi++] = (uint8_t)len;

    memcpy(&frame[fi], payload, len);
    fi += len;

    uint16_t crc = crc16(payload, len);
    frame[fi++] = (uint8_t)(crc >> 8);
    frame[fi++] = (uint8_t)(crc & 0xFF);
    frame[fi++] = 0x03;

    _serial->write(frame, fi);
    return true;
}

// ------------------------------------------------------------------------------
uint16_t VescUart::receivePacket(uint8_t* buf, uint16_t buf_size) {
    if (!_serial) return 0;

    uint32_t deadline = millis() + VESC_TIMEOUT_MS;
    uint8_t  start = 0;

    // Wait for start byte
    while (millis() < deadline) {
        if (_serial->available()) {
            start = _serial->read();
            if (start == 0x02 || start == 0x03) break;
        }
        yield();
    }
    if (start != 0x02 && start != 0x03) return 0;

    // Read length — 1 byte for short frame (0x02 start)
    uint16_t payload_len = 0;
    if (start == 0x02) {
        uint8_t lb;
        if (_serial->readBytes(&lb, 1) != 1) return 0;
        payload_len = lb;
    } else {
        uint8_t lb[2];
        if (_serial->readBytes(lb, 2) != 2) return 0;
        payload_len = ((uint16_t)lb[0] << 8) | lb[1];
    }

    if (payload_len == 0 || payload_len > buf_size) return 0;

    // Read payload
    uint16_t got = _serial->readBytes(buf, payload_len);
    if (got != payload_len) return 0;

    // Read CRC + end byte (3 bytes)
    uint8_t trailer[3];
    if (_serial->readBytes(trailer, 3) != 3) return 0;
    if (trailer[2] != 0x03) return 0;   // end byte

    uint16_t rx_crc = ((uint16_t)trailer[0] << 8) | trailer[1];
    if (rx_crc != crc16(buf, payload_len)) return 0;   // CRC mismatch

    return payload_len;
}

// ==============================================================================
// Private — CRC & buffer helpers
// ==============================================================================

uint16_t VescUart::crc16(const uint8_t* buf, uint16_t len) {
    uint16_t crc = 0;
    while (len--) {
        crc = CRC16_TAB[((crc >> 8) ^ *buf++) & 0xFF] ^ (crc << 8);
    }
    return crc;
}

float VescUart::bufGetFloat32(const uint8_t* buf, float scale, int* idx) {
    int32_t val = ((int32_t)buf[*idx]     << 24)
                | ((int32_t)buf[*idx + 1] << 16)
                | ((int32_t)buf[*idx + 2] <<  8)
                | ((int32_t)buf[*idx + 3]);
    *idx += 4;
    return (float)val / scale;
}

float VescUart::bufGetFloat16(const uint8_t* buf, float scale, int* idx) {
    int16_t val = ((int16_t)buf[*idx] << 8) | buf[*idx + 1];
    *idx += 2;
    return (float)val / scale;
}

int32_t VescUart::bufGetInt32(const uint8_t* buf, int* idx) {
    int32_t val = ((int32_t)buf[*idx]     << 24)
                | ((int32_t)buf[*idx + 1] << 16)
                | ((int32_t)buf[*idx + 2] <<  8)
                | ((int32_t)buf[*idx + 3]);
    *idx += 4;
    return val;
}

int16_t VescUart::bufGetInt16(const uint8_t* buf, int* idx) {
    int16_t val = ((int16_t)buf[*idx] << 8) | buf[*idx + 1];
    *idx += 2;
    return val;
}

void VescUart::bufAppendFloat32(uint8_t* buf, float val, float scale, int* idx) {
    int32_t i = (int32_t)(val * scale);
    buf[(*idx)++] = (uint8_t)(i >> 24);
    buf[(*idx)++] = (uint8_t)(i >> 16);
    buf[(*idx)++] = (uint8_t)(i >>  8);
    buf[(*idx)++] = (uint8_t)(i      );
}

void VescUart::bufAppendInt32(uint8_t* buf, int32_t val, int* idx) {
    buf[(*idx)++] = (uint8_t)(val >> 24);
    buf[(*idx)++] = (uint8_t)(val >> 16);
    buf[(*idx)++] = (uint8_t)(val >>  8);
    buf[(*idx)++] = (uint8_t)(val      );
}
