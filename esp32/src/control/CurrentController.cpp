#include "control/CurrentController.h"

#include <math.h>

namespace scooter {

CurrentControlOutput CurrentController::update(const CurrentControlInputs &inputs,
                                               const CurrentControlProfile &profile,
                                               const VescTelemetry &telemetry,
                                               uint32_t nowMs) {
    float dtSeconds = 0.01f;
    if (_lastTickMs != 0 && nowMs > _lastTickMs) {
        dtSeconds = static_cast<float>(nowMs - _lastTickMs) / 1000.0f;
    }
    _lastTickMs = nowMs;

    CurrentControlOutput output;
    output.speedScale = computeSpeedScale(profile, telemetry);

    float driveTarget = 0.0f;
    float brakeTarget = 0.0f;

    if (!inputs.enabled || inputs.emergencyStop) {
        driveTarget = 0.0f;
        brakeTarget = 0.0f;
    } else if (clamp01(inputs.brake) > 0.0f) {
        brakeTarget = clamp01(inputs.brake) * profile.maxBrakeCurrentA;
    } else {
        driveTarget = clamp01(inputs.throttle) * profile.maxDriveCurrentA * output.speedScale;
    }

    output.driveTargetCurrentA = driveTarget;
    output.brakeTargetCurrentA = brakeTarget;

    const float driveStep = profile.driveRampRateAps * dtSeconds;
    const float brakeStep = profile.brakeRampRateAps * dtSeconds;

    _driveCurrentA = slew(_driveCurrentA, driveTarget, driveStep);
    _brakeCurrentA = slew(_brakeCurrentA, brakeTarget, brakeStep);

    if (_brakeCurrentA > 0.01f) {
        _driveCurrentA = 0.0f;
    }
    if (_driveCurrentA > 0.01f) {
        _brakeCurrentA = 0.0f;
    }

    output.driveCurrentA = _driveCurrentA;
    output.brakeCurrentA = _brakeCurrentA;
    return output;
}

void CurrentController::reset() {
    _driveCurrentA = 0.0f;
    _brakeCurrentA = 0.0f;
    _lastTickMs = 0;
}

float CurrentController::clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float CurrentController::slew(float current, float target, float maxStep) {
    if (current < target) {
        current += maxStep;
        if (current > target) {
            current = target;
        }
    } else if (current > target) {
        current -= maxStep;
        if (current < target) {
            current = target;
        }
    }
    return current;
}

float CurrentController::computeSpeedScale(const CurrentControlProfile &profile, const VescTelemetry &telemetry) {
    if (!telemetry.valid || profile.speedLimitRpm <= 0.0f || profile.speedTaperStartRpm <= 0.0f) {
        return 1.0f;
    }

    const float rpm = fabsf(static_cast<float>(telemetry.rpm));
    if (rpm <= profile.speedTaperStartRpm) {
        return 1.0f;
    }
    if (rpm >= profile.speedLimitRpm) {
        return 0.0f;
    }

    const float span = profile.speedLimitRpm - profile.speedTaperStartRpm;
    if (span <= 0.0f) {
        return 0.0f;
    }

    const float remaining = profile.speedLimitRpm - rpm;
    return remaining / span;
}

}  // namespace scooter