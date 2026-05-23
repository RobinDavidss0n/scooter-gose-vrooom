#include "cst816s.h"

bool CST816S::begin(uint8_t sda, uint8_t scl, uint8_t int_pin, uint8_t rst_pin) {
    _int_pin = int_pin;
    _rst_pin = rst_pin;

    pinMode(_rst_pin, OUTPUT);
    pinMode(_int_pin, INPUT_PULLUP);

    // Hardware reset sequence
    digitalWrite(_rst_pin, LOW);
    delay(10);
    digitalWrite(_rst_pin, HIGH);
    delay(50);

    Wire.begin(sda, scl, 400000UL);  // 400 kHz

    // Verify chip ID (CST816S = 0xB4 or 0xB5)
    uint8_t id = readByte(REG_CHIP_ID);
    if (id != 0xB4 && id != 0xB5 && id != 0x16) {
        Serial.printf("[TOUCH] CST816S not found (got 0x%02X)\n", id);
        return false;
    }
    Serial.printf("[TOUCH] CST816S found, chip_id=0x%02X\n", id);

    // Enable continuous interrupt mode (bit[0]=1 in IRQ_CTL)
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_IRQ_CTL);
    Wire.write(0x01);
    Wire.endTransmission();

    return true;
}

// ------------------------------------------------------------------------------
TouchPoint CST816S::read() {
    uint8_t buf[6];
    if (!readBlock(REG_GESTURE, buf, 6)) {
        _point.touched = false;
        return _point;
    }

    _point.gesture = static_cast<CST816S_Gesture>(buf[0]);
    _point.fingers = buf[1] & 0x0F;
    _point.x       = (int16_t)(((buf[2] & 0x0F) << 8) | buf[3]);
    _point.y       = (int16_t)(((buf[4] & 0x0F) << 8) | buf[5]);
    _point.touched = (_point.fingers > 0);

    return _point;
}

// ------------------------------------------------------------------------------
uint8_t CST816S::readByte(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

bool CST816S::readBlock(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom(I2C_ADDR, len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}
