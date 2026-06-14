#include "throttle.h"

int      throttleRawAdc     = 0;
uint16_t throttleMillivolts = 0;
float    throttleNormalized = 0.0f;
bool     throttleLogEnabled = true;
uint32_t lastThrottleLogMs  = 0;

float normalize_throttle_mv(uint16_t millivolts)
{
    const float startMv = static_cast<float>(THROTTLE_IDLE_MV + THROTTLE_IDLE_DEADZONE_MV);
    const float endMv   = static_cast<float>(THROTTLE_FULL_MV);
    if (endMv <= startMv) {
        return 0.0f;
    }
    return clamp01((static_cast<float>(millivolts) - startMv) / (endMv - startMv));
}

void read_physical_throttle(scooter::CurrentControlInputs &inputs)
{
    throttleRawAdc     = analogRead(THROTTLE_ADC_PIN);
    throttleMillivolts = static_cast<uint16_t>(analogReadMilliVolts(THROTTLE_ADC_PIN));
    throttleNormalized = normalize_throttle_mv(throttleMillivolts);

    if (inputs.enabled) {
        inputs.throttle = throttleNormalized;
        inputs.brake    = 0.0f;
    }
}

void log_physical_throttle(uint32_t nowMs, bool controlEnabled)
{
    if (!throttleLogEnabled) {
        return;
    }
    if (nowMs - lastThrottleLogMs < THROTTLE_LOG_INTERVAL_MS) {
        return;
    }
    if (throttleNormalized < 0.05f) {
        return;
    }

    lastThrottleLogMs = nowMs;
    Serial.printf("throttle adc=%d mv=%u norm=%.3f enabled=%d\n",
                  throttleRawAdc,
                  static_cast<unsigned>(throttleMillivolts),
                  throttleNormalized,
                  controlEnabled);
}
