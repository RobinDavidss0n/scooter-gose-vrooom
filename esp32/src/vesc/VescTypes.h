#pragma once

#include <Arduino.h>

namespace scooter {

// ---------------------------------------------------------------------------
// Wire command IDs (VESC serial protocol)
// ---------------------------------------------------------------------------

enum class VescCommandId : uint8_t {
    GetValues       = 4,
    SetCurrent      = 6,
    SetCurrentBrake = 7,
    Alive           = 30,
};

// ---------------------------------------------------------------------------
// Decoded telemetry from a COMM_GET_VALUES response
// ---------------------------------------------------------------------------

struct VescTelemetry {
    // Validity
    bool     valid          = false;
    uint32_t lastResponseMs = 0;

    // Temperatures
    float mosfetTempC = NAN;
    float motorTempC  = NAN;

    // Currents
    float motorCurrentA = 0.0f;
    float inputCurrentA = 0.0f;
    float currentIdA    = 0.0f;
    float currentIqA    = 0.0f;

    // Motion
    float   dutyCycle = 0.0f;
    int32_t rpm       = 0;

    // Voltage
    float inputVoltageV = 0.0f;

    // Energy accounting
    float ampHours         = 0.0f;
    float ampHoursCharged  = 0.0f;
    float wattHours        = 0.0f;
    float wattHoursCharged = 0.0f;

    // Position
    int32_t tachometer    = 0;
    int32_t tachometerAbs = 0;
    float   pidPosition   = 0.0f;

    // Status
    uint8_t faultCode    = 0;
    uint8_t controllerId = 0;
};

}  // namespace scooter