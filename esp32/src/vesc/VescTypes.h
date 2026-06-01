#pragma once

#include <Arduino.h>

namespace scooter {

enum class VescCommandId : uint8_t {
    GetValues = 4,
    SetCurrent = 6,
    SetCurrentBrake = 7,
    Alive = 30,
};

struct VescTelemetry {
    bool valid = false;
    uint32_t lastResponseMs = 0;
    float mosfetTempC = NAN;
    float motorTempC = NAN;
    float motorCurrentA = 0.0f;
    float inputCurrentA = 0.0f;
    float currentIdA = 0.0f;
    float currentIqA = 0.0f;
    float dutyCycle = 0.0f;
    int32_t rpm = 0;
    float inputVoltageV = 0.0f;
    float ampHours = 0.0f;
    float ampHoursCharged = 0.0f;
    float wattHours = 0.0f;
    float wattHoursCharged = 0.0f;
    int32_t tachometer = 0;
    int32_t tachometerAbs = 0;
    uint8_t faultCode = 0;
    float pidPosition = 0.0f;
    uint8_t controllerId = 0;
};

}  // namespace scooter