#pragma once

#include <Arduino.h>

#include "vesc/VescTypes.h"

namespace scooter {

struct CurrentControlInputs {
    float throttle = 0.0f;
    float brake = 0.0f;
    bool enabled = false;
    bool emergencyStop = false;
};

struct CurrentControlProfile {
    float maxDriveCurrentA = 6.0f;
    float maxBrakeCurrentA = 4.0f;
    float speedTaperStartRpm = 5600.0f;
    float speedLimitRpm = 6250.0f;
    float driveRampRateAps = 25.0f;
    float brakeRampRateAps = 35.0f;
};

struct CurrentControlOutput {
    float driveCurrentA = 0.0f;
    float brakeCurrentA = 0.0f;
    float speedScale = 1.0f;
    float driveTargetCurrentA = 0.0f;
    float brakeTargetCurrentA = 0.0f;
};

class CurrentController {
public:
    CurrentControlOutput update(const CurrentControlInputs &inputs,
                                const CurrentControlProfile &profile,
                                const VescTelemetry &telemetry,
                                uint32_t nowMs);
    void reset();

private:
    float _driveCurrentA = 0.0f;
    float _brakeCurrentA = 0.0f;
    uint32_t _lastTickMs = 0;

    static float clamp01(float value);
    static float slew(float current, float target, float maxStep);
    static float computeSpeedScale(const CurrentControlProfile &profile, const VescTelemetry &telemetry);
};

}  // namespace scooter