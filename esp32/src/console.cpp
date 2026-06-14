#include "console.h"

static char   s_lineBuffer[96] = {0};
static size_t s_lineLength     = 0;

void print_console_help()
{
    Serial.println();
    Serial.println(F("USB console commands:"));
    Serial.println(F("  help"));
    Serial.println(F("  status"));
    Serial.println(F("  enable 0|1"));
    Serial.println(F("  throttle <0.0..1.0>"));
    Serial.println(F("  throttle_log 0|1"));
    Serial.println(F("  brake <0.0..1.0>"));
    Serial.println(F("  stop"));
    Serial.println(F("  profile drive <amps>"));
    Serial.println(F("  profile brake <amps>"));
    Serial.println(F("  profile speed <taper_kmh> <limit_kmh>"));
    Serial.println();
}

void print_status(scooter::ConsoleContext &ctx, uint32_t nowMs)
{
    const scooter::VescTelemetry &telemetry = ctx.vesc.telemetry();

    Serial.printf("enabled=%d throttle=%.2f brake=%.2f drive=%.2fA brake=%.2fA\n",
                  ctx.inputs.enabled,
                  ctx.inputs.throttle,
                  ctx.inputs.brake,
                  ctx.output.driveCurrentA,
                  ctx.output.brakeCurrentA);

    Serial.printf("link=%s telemetry=%s rpm=%ld vin=%.1fV motor=%.2fA input=%.2fA fault=%u age=%lu ms\n",
                  ctx.vesc.isConnected(nowMs) ? "online" : "offline",
                  ctx.vesc.hasFreshTelemetry(nowMs) ? "fresh" : "stale",
                  static_cast<long>(telemetry.rpm),
                  telemetry.inputVoltageV,
                  telemetry.motorCurrentA,
                  telemetry.inputCurrentA,
                  telemetry.faultCode,
                  telemetry.valid
                      ? static_cast<unsigned long>(nowMs - telemetry.lastResponseMs)
                      : 0UL);

    const float taperKmh = ctx.profile.speedTaperStartRpm * kRpmToKmhFactor;
    const float limitKmh = ctx.profile.speedLimitRpm * kRpmToKmhFactor;
    Serial.printf("profile drive=%.1fA brake=%.1fA taper=%.1f km/h limit=%.1f km/h\n",
                  ctx.profile.maxDriveCurrentA,
                  ctx.profile.maxBrakeCurrentA,
                  taperKmh,
                  limitKmh);
}

static void process_console_command(char *line, scooter::ConsoleContext &ctx)
{
    char *save    = nullptr;
    char *command = strtok_r(line, " \t", &save);
    if (command == nullptr) {
        return;
    }

    if (strcmp(command, "help") == 0) {
        print_console_help();
        return;
    }

    if (strcmp(command, "status") == 0) {
        print_status(ctx, millis());
        return;
    }

    if (strcmp(command, "stop") == 0) {
        ctx.inputs.throttle = 0.0f;
        ctx.inputs.brake    = 0.0f;
        ctx.controller.reset();
        Serial.println(F("Commanded stop."));
        return;
    }

    if (strcmp(command, "enable") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: enable 0|1"));
            return;
        }
        ctx.inputs.enabled = atoi(value) != 0;
        if (!ctx.inputs.enabled) {
            ctx.inputs.throttle = 0.0f;
            ctx.inputs.brake    = 0.0f;
            ctx.controller.reset();
        }
        Serial.printf("Controller %s.\n", ctx.inputs.enabled ? "enabled" : "disabled");
        return;
    }

    if (strcmp(command, "throttle") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: throttle <0.0..1.0>"));
            return;
        }
        ctx.inputs.throttle = clamp01(static_cast<float>(atof(value)));
        ctx.inputs.brake    = 0.0f;
        Serial.printf("Throttle set to %.2f.\n", ctx.inputs.throttle);
        return;
    }

    if (strcmp(command, "throttle_log") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: throttle_log 0|1"));
            return;
        }
        ctx.throttleLogEnabled = atoi(value) != 0;
        Serial.printf("Throttle logging %s.\n",
                      ctx.throttleLogEnabled ? "enabled" : "disabled");
        return;
    }

    if (strcmp(command, "brake") == 0) {
        char *value = strtok_r(nullptr, " \t", &save);
        if (value == nullptr) {
            Serial.println(F("Usage: brake <0.0..1.0>"));
            return;
        }
        ctx.inputs.brake    = clamp01(static_cast<float>(atof(value)));
        ctx.inputs.throttle = 0.0f;
        Serial.printf("Brake set to %.2f.\n", ctx.inputs.brake);
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
            ctx.profile.maxDriveCurrentA = max(0.0f, static_cast<float>(atof(value)));
            Serial.printf("Drive current limit set to %.1f A.\n",
                          ctx.profile.maxDriveCurrentA);
            return;
        }

        if (strcmp(field, "brake") == 0) {
            char *value = strtok_r(nullptr, " \t", &save);
            if (value == nullptr) {
                Serial.println(F("Usage: profile brake <amps>"));
                return;
            }
            ctx.profile.maxBrakeCurrentA = max(0.0f, static_cast<float>(atof(value)));
            Serial.printf("Brake current limit set to %.1f A.\n",
                          ctx.profile.maxBrakeCurrentA);
            return;
        }

        if (strcmp(field, "speed") == 0) {
            char *taperValue = strtok_r(nullptr, " \t", &save);
            char *limitValue = strtok_r(nullptr, " \t", &save);
            if (taperValue == nullptr || limitValue == nullptr) {
                Serial.println(F("Usage: profile speed <taper_kmh> <limit_kmh>"));
                return;
            }
            const float taperKmh = max(0.0f, static_cast<float>(atof(taperValue)));
            const float limitKmh = max(taperKmh, static_cast<float>(atof(limitValue)));
            ctx.profile.speedTaperStartRpm = taperKmh / kRpmToKmhFactor;
            ctx.profile.speedLimitRpm      = limitKmh / kRpmToKmhFactor;
            Serial.printf("Speed taper %.1f km/h, hard limit %.1f km/h.\n",
                          taperKmh, limitKmh);
            return;
        }

        Serial.println(F("Unknown profile field."));
        return;
    }

    Serial.println(F("Unknown command. Type 'help'."));
}

void poll_console(scooter::ConsoleContext &ctx)
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
            s_lineBuffer[s_lineLength] = '\0';
            if (s_lineLength > 0) {
                char lineCopy[sizeof(s_lineBuffer)] = {0};
                strncpy(lineCopy, s_lineBuffer, sizeof(lineCopy) - 1);
                process_console_command(lineCopy, ctx);
            }
            s_lineLength = 0;
            continue;
        }

        if (s_lineLength + 1 < sizeof(s_lineBuffer)) {
            s_lineBuffer[s_lineLength++] = ch;
        }
    }
}
