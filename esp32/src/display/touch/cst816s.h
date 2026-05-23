#pragma once
#include <Arduino.h>
#include <Wire.h>

// ==============================================================================
// CST816S capacitive touch controller driver (I2C)
// Used on Waveshare ESP32-S3 1.28" Round Touch LCD
// I2C address: 0x15
// ==============================================================================

// Gesture IDs reported by the chip
enum CST816S_Gesture : uint8_t {
    GESTURE_NONE       = 0x00,
    GESTURE_SWIPE_UP   = 0x01,
    GESTURE_SWIPE_DOWN = 0x02,
    GESTURE_SWIPE_LEFT = 0x03,
    GESTURE_SWIPE_RIGHT= 0x04,
    GESTURE_CLICK      = 0x05,
    GESTURE_DOUBLE_CLICK = 0x0B,
    GESTURE_LONG_PRESS = 0x0C,
};

struct TouchPoint {
    int16_t  x        = 0;
    int16_t  y        = 0;
    uint8_t  fingers  = 0;
    CST816S_Gesture gesture = GESTURE_NONE;
    bool     touched  = false;
};

class CST816S {
public:
    // Initialise I2C and reset the chip.
    // Returns true if the device is found on the bus.
    bool begin(uint8_t sda, uint8_t scl, uint8_t int_pin, uint8_t rst_pin);

    // Read latest touch state (call frequently, or trigger from INT pin ISR)
    TouchPoint read();

    // Raw access
    bool    isTouched()  const { return _point.touched; }
    int16_t getX()       const { return _point.x; }
    int16_t getY()       const { return _point.y; }
    CST816S_Gesture getGesture() const { return _point.gesture; }

private:
    static constexpr uint8_t I2C_ADDR     = 0x15;
    static constexpr uint8_t REG_GESTURE  = 0x01;
    static constexpr uint8_t REG_FINGERS  = 0x02;
    static constexpr uint8_t REG_XH       = 0x03;
    static constexpr uint8_t REG_XL       = 0x04;
    static constexpr uint8_t REG_YH       = 0x05;
    static constexpr uint8_t REG_YL       = 0x06;
    static constexpr uint8_t REG_CHIP_ID  = 0xA7;
    static constexpr uint8_t REG_IRQ_CTL  = 0xFA;

    uint8_t    _int_pin   = 0;
    uint8_t    _rst_pin   = 0;
    TouchPoint _point;

    uint8_t readByte(uint8_t reg);
    bool    readBlock(uint8_t reg, uint8_t* buf, uint8_t len);
};
