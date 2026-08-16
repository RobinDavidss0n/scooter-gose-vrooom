#include <Arduino.h>

#include "config.h"
#include "control/CurrentController.h"
#include "vesc/VescUart.h"
#include "throttle.h"
#include "console.h"
#include "lights.h"
#include "blinkers.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static HardwareSerial vescSerial(1);

scooter::VescUart            vesc(vescSerial, VESC_UART_RX_PIN, VESC_UART_TX_PIN, VESC_UART_BAUD);
scooter::CurrentController   currentController;
scooter::CurrentControlInputs  controlInputs;
scooter::CurrentControlProfile controlProfile;
scooter::CurrentControlOutput  controlOutput;

// ---------------------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------------------
static uint32_t lastControlTickMs      = 0;
static uint32_t lastTelemetryRequestMs = 0;
static uint32_t lastAliveMs            = 0;
static uint32_t lastUiTickMs           = 0;
static uint32_t lastLvglTickMs         = 0;

// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(250);

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH);

    pinMode(THROTTLE_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(THROTTLE_ADC_PIN, ADC_11db);

    setup_ui();
    lastLvglTickMs = millis();

    lights_init();
    blinkers_init();

    controlInputs.enabled            = false;
    controlInputs.throttle           = 0.0f;
    controlInputs.brake              = 0.0f;
    controlProfile.maxDriveCurrentA  = CONTROL_INITIAL_DRIVE_CURRENT_A;
    controlProfile.maxBrakeCurrentA  = CONTROL_INITIAL_BRAKE_CURRENT_A;
    controlProfile.speedTaperStartRpm = CONTROL_SPEED_TAPER_START_KMH / kRpmToKmhFactor;
    controlProfile.speedLimitRpm     = CONTROL_SPEED_LIMIT_KMH / kRpmToKmhFactor;
    controlProfile.driveRampRateAps  = CONTROL_DRIVE_RAMP_RATE_APS;
    controlProfile.brakeRampRateAps  = CONTROL_BRAKE_RAMP_RATE_APS;

    vesc.begin();
    vesc.sendCurrent(0.0f);
    vesc.sendAlive();

    print_console_help();
    Serial.printf("VESC UART on RX=%d TX=%d @ %lu baud\n",
                  VESC_UART_RX_PIN,
                  VESC_UART_TX_PIN,
                  static_cast<unsigned long>(VESC_UART_BAUD));
    Serial.printf("Throttle ADC on GPIO=%d idle=%u mV full=%u mV\n",
                  THROTTLE_ADC_PIN,
                  static_cast<unsigned>(THROTTLE_IDLE_MV),
                  static_cast<unsigned>(THROTTLE_FULL_MV));
    Serial.println("Done with setup.");
}

void loop()
{
    const uint32_t nowMs         = millis();
    const uint32_t lvglElapsedMs = nowMs - lastLvglTickMs;
    if (lvglElapsedMs > 0) {
        lv_tick_inc(lvglElapsedMs);
        lastLvglTickMs = nowMs;
    }

    scooter::ConsoleContext consoleCtx = {
        controlInputs,
        controlProfile,
        controlOutput,
        currentController,
        vesc,
        throttleLogEnabled,
        unrestrictedModeActive
    };
    poll_console(consoleCtx);

    blinkers_poll(nowMs, controlProfile);

    read_physical_throttle(controlInputs);
    log_physical_throttle(nowMs, controlInputs.enabled);
    vesc.poll(nowMs);

    if (nowMs - lastControlTickMs >= CONTROL_INTERVAL_MS) {
        lastControlTickMs = nowMs;
        controlOutput = currentController.update(
            controlInputs, controlProfile, vesc.telemetry(), nowMs);
        if (controlOutput.brakeCurrentA > 0.01f) {
            vesc.sendBrakeCurrent(controlOutput.brakeCurrentA);
        } else {
            vesc.sendCurrent(controlOutput.driveCurrentA);
        }
        update_lights(controlOutput.brakeCurrentA > 0.01f);
    }

    if (nowMs - lastTelemetryRequestMs >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryRequestMs = nowMs;
        vesc.requestTelemetry();
    }

    if (nowMs - lastAliveMs >= ALIVE_INTERVAL_MS) {
        lastAliveMs = nowMs;
        vesc.sendAlive();
    }

    if (nowMs - lastUiTickMs >= UI_INTERVAL_MS) {
        lastUiTickMs = nowMs;
        update_ui(nowMs, vesc);
    }

    lv_timer_handler();
    delay(2);
}