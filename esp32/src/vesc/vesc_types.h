#pragma once
#include <stdint.h>

// ==============================================================================
// VESC data types and protocol constants
// Based on VESC firmware: https://github.com/vedderb/bldc
// ==============================================================================

// Packet command IDs
enum COMM_PACKET_ID : uint8_t {
    COMM_FW_VERSION              = 0,
    COMM_JUMP_TO_BOOTLOADER      = 1,
    COMM_ERASE_NEW_APP           = 2,
    COMM_WRITE_NEW_APP_DATA      = 3,
    COMM_GET_VALUES              = 4,
    COMM_SET_DUTY                = 5,
    COMM_SET_CURRENT             = 6,
    COMM_SET_CURRENT_BRAKE       = 7,
    COMM_SET_RPM                 = 8,
    COMM_SET_POS                 = 9,
    COMM_SET_HANDBRAKE           = 10,
    COMM_GET_VALUES_SETUP        = 50,
    COMM_ALIVE                   = 30,
    COMM_GET_DECODED_PPM         = 18,
    COMM_GET_DECODED_ADC         = 19,
    COMM_SET_CURRENT_LIMIT       = 93,
};

// Motor fault codes returned in COMM_GET_VALUES
enum mc_fault_code : uint8_t {
    FAULT_CODE_NONE                     = 0,
    FAULT_CODE_OVER_VOLTAGE             = 1,
    FAULT_CODE_UNDER_VOLTAGE            = 2,
    FAULT_CODE_DRV                      = 3,
    FAULT_CODE_ABS_OVER_CURRENT         = 4,
    FAULT_CODE_OVER_TEMP_FET            = 5,
    FAULT_CODE_OVER_TEMP_MOTOR          = 6,
    FAULT_CODE_GATE_DRIVER_OVER_VOLTAGE = 7,
    FAULT_CODE_GATE_DRIVER_UNDER_VOLTAGE = 8,
    FAULT_CODE_MCU_UNDER_VOLTAGE        = 9,
    FAULT_CODE_BOOTING_FROM_WATCHDOG_RESET = 10,
    FAULT_CODE_ENCODER_SPI              = 11,
    FAULT_CODE_ENCODER_SINCOS_BELOW_MIN_AMPLITUDE = 12,
    FAULT_CODE_ENCODER_SINCOS_ABOVE_MAX_AMPLITUDE = 13,
    FAULT_CODE_FLASH_CORRUPTION         = 14,
    FAULT_CODE_HIGH_OFFSET_CURRENT_SENSOR_1 = 15,
    FAULT_CODE_HIGH_OFFSET_CURRENT_SENSOR_2 = 16,
    FAULT_CODE_HIGH_OFFSET_CURRENT_SENSOR_3 = 17,
    FAULT_CODE_UNBALANCED_CURRENTS      = 18,
};

// Human-readable fault strings
inline const char* faultString(mc_fault_code fc) {
    switch (fc) {
        case FAULT_CODE_NONE:                      return "OK";
        case FAULT_CODE_OVER_VOLTAGE:              return "Over Voltage";
        case FAULT_CODE_UNDER_VOLTAGE:             return "Under Voltage";
        case FAULT_CODE_DRV:                       return "DRV Fault";
        case FAULT_CODE_ABS_OVER_CURRENT:          return "Overcurrent";
        case FAULT_CODE_OVER_TEMP_FET:             return "FET Overtemp";
        case FAULT_CODE_OVER_TEMP_MOTOR:           return "Motor Overtemp";
        case FAULT_CODE_MCU_UNDER_VOLTAGE:         return "MCU Under Voltage";
        case FAULT_CODE_BOOTING_FROM_WATCHDOG_RESET: return "Watchdog Reset";
        default:                                   return "Unknown Fault";
    }
}

// Full telemetry snapshot from COMM_GET_VALUES
struct VescValues {
    float    temp_mos          = 0.0f;   // MOSFET temperature (°C)
    float    temp_motor        = 0.0f;   // Motor temperature (°C, needs NTC)
    float    avg_motor_current = 0.0f;   // Average motor current (A)
    float    avg_input_current = 0.0f;   // Average battery current (A)
    float    avg_id            = 0.0f;   // D-axis current (A)
    float    avg_iq            = 0.0f;   // Q-axis current (A)
    float    duty_cycle        = 0.0f;   // Duty cycle (-1.0 … 1.0)
    float    rpm               = 0.0f;   // Electrical RPM
    float    v_in              = 0.0f;   // Input voltage (V)
    float    amp_hours         = 0.0f;   // Total amp-hours consumed
    float    amp_hours_charged = 0.0f;   // Total amp-hours charged (regen)
    float    watt_hours        = 0.0f;   // Total watt-hours consumed
    float    watt_hours_charged= 0.0f;   // Total watt-hours charged (regen)
    int32_t  tachometer        = 0;      // Tachometer (electrical counts)
    int32_t  tachometer_abs    = 0;      // Tachometer absolute
    mc_fault_code fault_code   = FAULT_CODE_NONE;

    // Derived / convenience fields (populated by VescUart after decode)
    float    speed_kmh         = 0.0f;   // Calculated from RPM + motor geometry
    int      battery_pct       = 0;      // 0–100, from v_in
    float    power_w           = 0.0f;   // avg_input_current × v_in
    bool     valid             = false;  // true = data was successfully decoded
};

// Firmware version info (COMM_FW_VERSION response)
struct VescFwVersion {
    uint8_t  major     = 0;
    uint8_t  minor     = 0;
    char     hw_name[20] = {};
    bool     valid     = false;
};
