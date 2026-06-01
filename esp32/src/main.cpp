#include <Arduino.h>
#include <math.h>
#include <string.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "control/CurrentController.h"
#include "vesc/VescUart.h"

// --- Correct 1.28" Microcontroller Hardware Pin Map ---
#define LCD_BL_PIN   2   // Direct MCU pin for Backlight PWM control
#define LCD_RST_PIN 14   // Direct MCU pin for Display Hardware Reset

constexpr int VESC_UART_RX_PIN = 17;
constexpr int VESC_UART_TX_PIN = 18;
constexpr uint32_t VESC_UART_BAUD = 115200;

constexpr uint32_t CONTROL_INTERVAL_MS = 10;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 100;
constexpr uint32_t ALIVE_INTERVAL_MS = 200;
constexpr uint32_t UI_INTERVAL_MS = 100;

// 1. Build the explicit Hardware Driver wrapper for the 1.28" Board Layout
class LGFX_Waveshare_128 : public lgfx::LGFX_Device 
{
    lgfx::Panel_GC9A01  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
public:
    LGFX_Waveshare_128() 
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.pin_sclk = 10;  
            cfg.pin_mosi = 11;  
            cfg.pin_miso = 12;  
            cfg.pin_dc   = 8;   
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.panel_width    = 240;
            cfg.panel_height   = 240;
            cfg.pin_cs         = 9;   
            cfg.pin_rst        = LCD_RST_PIN; // Give LovyanGFX control of the hardware reset pin

            cfg.rgb_order      = true;

            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

LGFX_Waveshare_128 lcd;
HardwareSerial vescSerial(1);
scooter::VescUart vesc(vescSerial, VESC_UART_RX_PIN, VESC_UART_TX_PIN, VESC_UART_BAUD);
scooter::CurrentController currentController;
scooter::CurrentControlInputs controlInputs;
scooter::CurrentControlProfile controlProfile;
scooter::CurrentControlOutput controlOutput;

uint32_t lastControlTickMs = 0;
uint32_t lastTelemetryRequestMs = 0;
uint32_t lastAliveMs = 0;
uint32_t lastUiTickMs = 0;

lv_obj_t *title_label = nullptr;
lv_obj_t *status_label = nullptr;
lv_obj_t *hint_label = nullptr;

char consoleLineBuffer[96] = {0};
size_t consoleLineLength = 0;

// 2. LVGL Graphics Integration Framebuffer Hooks
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

LV_IMG_DECLARE(headlight);
LV_IMG_DECLARE(left_blinker);

void setup_headlight_icon()
{
    // Create the headlight icon object
    lv_obj_t * img_headlight = lv_img_create(lv_scr_act());
    lv_img_set_src(img_headlight, &headlight);

    lv_img_set_zoom(img_headlight, 20);
    
    // Position it slightly above the center of your round screen
    lv_obj_align(img_headlight, LV_ALIGN_CENTER, 0, -40);
}

void setup_left_blinker_icon()
{
    // Create the left_blinker icon object
    lv_obj_t * img_left_blinker = lv_img_create(lv_scr_act());
    lv_img_set_src(img_left_blinker, &left_blinker);

    // LVGL Zoom: 256 = 100%. Adjust this if you want to scale it down (e.g., 128 = 50%)
    lv_img_set_zoom(img_left_blinker, 20); 
    
    // Shift left (-55px) and slightly up (-25px) from the absolute center
    lv_obj_align(img_left_blinker, LV_ALIGN_CENTER, -55, -25);
}

void setup_right_blinker_icon()
{
    // Create the right_blinker icon object
    lv_obj_t * img_right_blinker = lv_img_create(lv_scr_act());
    lv_img_set_src(img_right_blinker, &left_blinker);

    // LVGL uses 0.1 degree units, so 180 degrees = 1800
    lv_img_set_angle(img_right_blinker, 1800);

    // LVGL Zoom: 256 = 100%. Keep matching scale with the left side
    lv_img_set_zoom(img_right_blinker, 20); 
    
    // Shift right (+55px) and slightly up (-25px) from the absolute center
    lv_obj_align(img_right_blinker, LV_ALIGN_CENTER, 55, -25);
}

static float clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

void print_console_help()
{
    Serial.println();
    Serial.println(F("USB console commands:"));
    Serial.println(F("  help"));
    Serial.println(F("  status"));
    Serial.println(F("  enable 0|1"));
    Serial.println(F("  throttle <0.0..1.0>"));
    Serial.println(F("  brake <0.0..1.0>"));
    Serial.println(F("  stop"));
    Serial.println(F("  profile drive <amps>"));
    Serial.println(F("  profile brake <amps>"));
    Serial.println(F("  profile speed <taper_rpm> <limit_rpm>"));
    Serial.println();
}

void print_status(uint32_t nowMs)
{
    const scooter::VescTelemetry &telemetry = vesc.telemetry();
    Serial.printf("enabled=%d throttle=%.2f brake=%.2f drive=%.2fA brake=%.2fA\n",
                  controlInputs.enabled,
                  controlInputs.throttle,
                  controlInputs.brake,
                  controlOutput.driveCurrentA,
                  controlOutput.brakeCurrentA);
    Serial.printf("link=%s telemetry=%s rpm=%ld vin=%.1fV motor=%.2fA input=%.2fA fault=%u age=%lu ms\n",
                  vesc.isConnected(nowMs) ? "online" : "offline",
                  vesc.hasFreshTelemetry(nowMs) ? "fresh" : "stale",
                  static_cast<long>(telemetry.rpm),
                  telemetry.inputVoltageV,
                  telemetry.motorCurrentA,
                  telemetry.inputCurrentA,
                  telemetry.faultCode,
                  telemetry.valid ? static_cast<unsigned long>(nowMs - telemetry.lastResponseMs) : 0UL);
    Serial.printf("profile drive=%.1fA brake=%.1fA taper=%.0f rpm limit=%.0f rpm\n",
                  controlProfile.maxDriveCurrentA,
                  controlProfile.maxBrakeCurrentA,
                  controlProfile.speedTaperStartRpm,
                  controlProfile.speedLimitRpm);
}

void process_console_command(char *line)
{
    char *save = nullptr;
    char *command = strtok_r(line, " \t", &save);
    if (command == nullptr) {
        return;
    }

    if (strcmp(command, "help") == 0) {
        print_console_help();
        return;
    }

    if (strcmp(command, "status") == 0) {
        print_status(millis());
        return;
    }

    if (strcmp(command, "stop") == 0) {
        controlInputs.throttle = 0.0f;
        controlInputs.brake = 0.0f;
        currentController.reset();
        Serial.println(F("Commanded stop."));
        return;
    }

    if (strcmp(command, "enable") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: enable 0|1"));
            return;
        }

        controlInputs.enabled = atoi(value) != 0;
        if (!controlInputs.enabled) {
            controlInputs.throttle = 0.0f;
            controlInputs.brake = 0.0f;
            currentController.reset();
        }
        Serial.printf("Controller %s.\n", controlInputs.enabled ? "enabled" : "disabled");
        return;
    }

    if (strcmp(command, "throttle") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: throttle <0.0..1.0>"));
            return;
        }

        controlInputs.throttle = clamp01(static_cast<float>(atof(value)));
        controlInputs.brake = 0.0f;
        Serial.printf("Throttle set to %.2f.\n", controlInputs.throttle);
        return;
    }

    if (strcmp(command, "brake") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: brake <0.0..1.0>"));
            return;
        }

        controlInputs.brake = clamp01(static_cast<float>(atof(value)));
        controlInputs.throttle = 0.0f;
        Serial.printf("Brake set to %.2f.\n", controlInputs.brake);
        return;
    }

    if (strcmp(command, "profile") == 0) {
        char *field = strtok_r(nullptr, " \t", &save);
        if (field == nullptr) {
            Serial.println(F("Usage: profile drive <amps> | brake <amps> | speed <taper> <limit>"));
            return;
        }

        if (strcmp(field, "drive") == 0) {
            char *value = strtok_r(nullptr, " \t", &save);
            if (value == nullptr) {
                Serial.println(F("Usage: profile drive <amps>"));
                return;
            }
            controlProfile.maxDriveCurrentA = max(0.0f, static_cast<float>(atof(value)));
            Serial.printf("Drive current limit set to %.1f A.\n", controlProfile.maxDriveCurrentA);
            return;
        }

        if (strcmp(field, "brake") == 0) {
            char *value = strtok_r(nullptr, " \t", &save);
            if (value == nullptr) {
                Serial.println(F("Usage: profile brake <amps>"));
                return;
            }
            controlProfile.maxBrakeCurrentA = max(0.0f, static_cast<float>(atof(value)));
            Serial.printf("Brake current limit set to %.1f A.\n", controlProfile.maxBrakeCurrentA);
            return;
        }

        if (strcmp(field, "speed") == 0) {
            char *taperValue = strtok_r(nullptr, " \t", &save);
            char *limitValue = strtok_r(nullptr, " \t", &save);
            if (taperValue == nullptr || limitValue == nullptr) {
                Serial.println(F("Usage: profile speed <taper_rpm> <limit_rpm>"));
                return;
            }

            controlProfile.speedTaperStartRpm = max(0.0f, static_cast<float>(atof(taperValue)));
            controlProfile.speedLimitRpm = max(controlProfile.speedTaperStartRpm, static_cast<float>(atof(limitValue)));
            Serial.printf("Speed taper %.0f rpm, hard limit %.0f rpm.\n",
                          controlProfile.speedTaperStartRpm,
                          controlProfile.speedLimitRpm);
            return;
        }

        Serial.println(F("Unknown profile field."));
        return;
    }

    Serial.println(F("Unknown command. Type 'help'."));
}

void poll_console()
{
    while (Serial.available() > 0) {
        const int raw = Serial.read();
        if (raw < 0) {
            break;
        }

        const char ch = static_cast<char>(raw);
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            consoleLineBuffer[consoleLineLength] = '\0';
            if (consoleLineLength > 0) {
                char lineCopy[sizeof(consoleLineBuffer)] = {0};
                strncpy(lineCopy, consoleLineBuffer, sizeof(lineCopy) - 1);
                process_console_command(lineCopy);
            }
            consoleLineLength = 0;
            continue;
        }

        if (consoleLineLength + 1 < sizeof(consoleLineBuffer)) {
            consoleLineBuffer[consoleLineLength++] = ch;
        }
    }
}

void setup_dashboard()
{
    title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "VESC Current Mode");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    status_label = lv_label_create(lv_scr_act());
    lv_obj_set_width(status_label, 220);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(status_label, "Booting...");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 46);

    hint_label = lv_label_create(lv_scr_act());
    lv_obj_set_width(hint_label, 220);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint_label, "USB: help | enable 1 | throttle 0.10");
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    setup_headlight_icon();
    setup_left_blinker_icon();
    setup_right_blinker_icon();
}

void update_dashboard(uint32_t nowMs)
{
    if (status_label == nullptr) {
        return;
    }

    const scooter::VescTelemetry &telemetry = vesc.telemetry();
    char statusText[320] = {0};
    snprintf(statusText,
             sizeof(statusText),
             "Link: %s  Telemetry: %s\nRPM: %ld  Vin: %.1f V\nMotor: %.2f A  Input: %.2f A\nCmd drive: %.2f A  brake: %.2f A\nThrottle: %.2f  Brake: %.2f  Fault: %u",
             vesc.isConnected(nowMs) ? "ONLINE" : "OFFLINE",
             vesc.hasFreshTelemetry(nowMs) ? "FRESH" : "STALE",
             static_cast<long>(telemetry.rpm),
             telemetry.inputVoltageV,
             telemetry.motorCurrentA,
             telemetry.inputCurrentA,
             controlOutput.driveCurrentA,
             controlOutput.brakeCurrentA,
             controlInputs.throttle,
             controlInputs.brake,
             telemetry.faultCode);
    lv_label_set_text(status_label, statusText);
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((uint16_t *)&color_p->full, w * h, true);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH); 

    lcd.init();
    
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    setup_dashboard();

    controlInputs.enabled = false;
    controlInputs.throttle = 0.0f;
    controlInputs.brake = 0.0f;
    controlProfile.maxDriveCurrentA = 6.0f;
    controlProfile.maxBrakeCurrentA = 4.0f;
    controlProfile.speedTaperStartRpm = 5600.0f;
    controlProfile.speedLimitRpm = 6250.0f;
    controlProfile.driveRampRateAps = 25.0f;
    controlProfile.brakeRampRateAps = 35.0f;

    vesc.begin();
    vesc.sendCurrent(0.0f);
    vesc.sendAlive();

    print_console_help();
    Serial.printf("VESC UART on RX=%d TX=%d @ %lu baud\n",
                  VESC_UART_RX_PIN,
                  VESC_UART_TX_PIN,
                  static_cast<unsigned long>(VESC_UART_BAUD));
    
    Serial.println("Done with setup.");
}

void loop()
{
    const uint32_t nowMs = millis();

    poll_console();
    vesc.poll(nowMs);

    if (nowMs - lastControlTickMs >= CONTROL_INTERVAL_MS) {
        lastControlTickMs = nowMs;
        controlOutput = currentController.update(controlInputs, controlProfile, vesc.telemetry(), nowMs);
        if (controlOutput.brakeCurrentA > 0.01f) {
            vesc.sendBrakeCurrent(controlOutput.brakeCurrentA);
        } else {
            vesc.sendCurrent(controlOutput.driveCurrentA);
        }
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
        update_dashboard(nowMs);
    }

    lv_timer_handler();
    delay(2);
}