// =============================================================================
// mcu_poc.ino — Minimal standalone POC for Waveshare ESP32-S3 1.28" Round LCD
//
// Purpose: prove the MCU can drive the scooter and show its speed.
//   - Touch the UPPER half of the screen  -> GAS  (max 3 A motor current)
//   - Touch the LOWER half of the screen  -> BRAKE (2 A regen)
//   - Don't touch                         -> COAST (0 A, motor freewheels)
//   - Live speed is drawn big in the middle.
//
// Speed cap: the ESP32 checks the actual speed from COMM_GET_VALUES and stops
// sending GAS the moment speed >= MAX_KMH (5 km/h).  The VESC's own l_max_erpm
// is a second hardware backstop — set it conservatively in VESC Tool FIRST.
//
// Why COMM_SET_CURRENT instead of COMM_SET_RPM?
//   COMM_SET_RPM (ID=8) activates the VESC PID speed controller which needs the
//   motor-detection wizard to have been run AND PID gains that are tuned for the
//   specific motor.  Without those, it can jerk, oscillate, or fail to start.
//   COMM_SET_CURRENT (ID=6) drives the motor at a fixed current — it works
//   straight after motor detection with no extra tuning and is much safer for
//   first tests.  Source: vedderb/bldc commands.c COMM_SET_CURRENT handler:
//     mc_interface_set_current((float)buffer_get_int32(data, &ind) / 1000.0)
//   So the sender must multiply amps × 1000 and send as big-endian int32.
//
// COMM_GET_VALUES payload byte map (verified against vedderb/bldc commands.c
// with full 0xFFFFFFFF mask, i.e. plain COMM_GET_VALUES request):
//   [0]     cmd  (echo = 0x04)
//   [1..2]  temp_fet    float16/10
//   [3..4]  temp_motor  float16/10
//   [5..8]  avg_motor_current  float32/100
//   [9..12] avg_input_current  float32/100
//   [13..16] avg_id  float32/100
//   [17..20] avg_iq  float32/100
//   [21..22] duty_cycle  float16/1000
//   [23..26] rpm   float32/1.0  (signed ERPM stored as int32 * 1.0)
//
// CST816S I2C registers (verified against DS-CST816T-V2.2 datasheet and
// koendv/cst816t driver):
//   0x01 gesture_id  0x02 finger_num
//   0x03 xpos_h [3:0]=x[11:8]  0x04 xpos_l = x[7:0]
//   0x05 ypos_h [3:0]=y[11:8]  0x06 ypos_l = y[7:0]
//   0xFE REG_DIS_AUTOSLEEP — write 0xFF to keep chip awake permanently
//                            (without this it sleeps after ~2 s idle!)
//
// Dependencies (PlatformIO lib_deps, same as esp32/platformio.ini):
//   bodmer/TFT_eSPI   (with GC9A01 build flags)
//   Wire (built-in)
//
// Wiring (matches hardware/pinout.md):
//   ESP32 GPIO17 (TX) -> VESC RX
//   ESP32 GPIO18 (RX) -> VESC TX
//   ESP32 GND         -> VESC GND
//   ESP32 5V          -> 5 V from XL7015 (do NOT power both from USB + DC-DC)
//
// THIS FILE IS STANDALONE. Copy it into its own PlatformIO project to flash.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>

// ---- Pins (Waveshare ESP32-S3 1.28" round, fixed on PCB) --------------------
static const int     PIN_VESC_TX   = 17;
static const int     PIN_VESC_RX   = 18;
static const int     PIN_TP_SDA    = 6;
static const int     PIN_TP_SCL    = 7;
static const int     PIN_TP_RST    = 5;
static const int     PIN_TP_INT    = 4;
static const uint8_t CST816S_ADDR  = 0x15;

// ---- Safety limits ----------------------------------------------------------
// Xiaomi Elite NE: 15 pole pairs, 8.5" wheel (216 mm dia → 0.6786 m circ)
//   5 km/h = 5000/60 m/s / 0.6786 m × 15 = ~1839 ERPM
static const float   MAX_KMH       = 5.0f;     // software speed cap
static const float   GAS_AMPS      = 3.0f;     // motor current while in GAS state
static const float   BRAKE_AMPS    = 2.0f;     // regen current while BRAKE
static const float   POLE_PAIRS    = 15.0f;
static const float   WHEEL_CIRC_M  = 0.216f * 3.14159f;  // 0.6786 m

// ---- VESC command IDs (from vedderb/bldc comm/commands.h) -------------------
static const uint8_t COMM_GET_VALUES        = 4;
static const uint8_t COMM_SET_CURRENT       = 6;   // int32 × 1000 = mA
static const uint8_t COMM_SET_CURRENT_BRAKE = 7;   // int32 × 1000 = mA
static const uint8_t COMM_ALIVE             = 30;

// ---- Globals ----------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
HardwareSerial VESC(1);
float    g_speed_kmh  = 0.0f;
uint32_t last_poll_ms = 0;
uint32_t last_draw_ms = 0;

// =============================================================================
// VESC packet layer — short frames only (payload ≤ 255 bytes).
//
// Frame format (from vedderb/bldc comm/packet.c):
//   [0x02][LEN 1 byte][PAYLOAD LEN bytes][CRC_H][CRC_L][0x03]
//
// CRC16-CCITT: poly 0x1021, init 0x0000 (bitwise, no table — small enough).
// =============================================================================
static uint16_t crc16(const uint8_t* buf, uint16_t len) {
    uint16_t crc = 0;
    while (len--) {
        crc ^= (uint16_t)(*buf++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
    return crc;
}

static void vescSend(const uint8_t* payload, uint8_t len) {
    uint8_t frame[260];
    uint8_t n = 0;
    frame[n++] = 0x02;
    frame[n++] = len;
    memcpy(&frame[n], payload, len);  n += len;
    uint16_t c = crc16(payload, len);
    frame[n++] = (uint8_t)(c >> 8);
    frame[n++] = (uint8_t)(c & 0xFF);
    frame[n++] = 0x03;
    VESC.write(frame, n);
}

// Read one short VESC reply frame into out[].  Returns payload length, 0 = fail.
// We only look for start byte 0x02 (short frame).
// 0x03 is the END byte of frames — if we accept it as a start byte we'd
// misinterpret leftover end-bytes from the previous reply as a long-frame
// header, corrupting all subsequent reads.
static uint16_t vescRecv(uint8_t* out, uint16_t out_size, uint32_t timeout_ms = 80) {
    uint32_t deadline = millis() + timeout_ms;

    // Drain bytes until we see 0x02 (short-frame start) or timeout
    uint8_t start = 0;
    while (millis() < deadline) {
        if (VESC.available()) {
            start = (uint8_t)VESC.read();
            if (start == 0x02) break;
            // Any other byte (including 0x03 end-bytes) is noise — skip it
        }
        delayMicroseconds(100);
    }
    if (start != 0x02) return 0;

    // Read length byte
    while (millis() < deadline && !VESC.available()) delayMicroseconds(100);
    if (!VESC.available()) return 0;
    uint8_t plen = (uint8_t)VESC.read();
    if (plen == 0 || plen > out_size) return 0;

    // Read payload
    size_t got = VESC.readBytes(out, plen);
    if (got != plen) return 0;

    // Read [CRC_H][CRC_L][0x03]
    uint8_t trailer[3];
    if (VESC.readBytes(trailer, 3) != 3) return 0;
    if (trailer[2] != 0x03) return 0;

    uint16_t rx_crc = ((uint16_t)trailer[0] << 8) | trailer[1];
    if (rx_crc != crc16(out, plen)) return 0;
    return plen;
}

// ---- VESC command helpers ---------------------------------------------------

// COMM_SET_CURRENT: firmware divides received int32 by 1000.0 to get amps.
// Positive = drive, 0 = freewheel, negative = regen (use COMM_SET_CURRENT_BRAKE
// for explicit braking — it maps to mc_interface_set_brake_current which only
// accepts positive values).
static void vescSetCurrent(float amps) {
    int32_t v = (int32_t)(amps * 1000.0f);
    uint8_t p[5] = { COMM_SET_CURRENT,
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v) };
    vescSend(p, 5);
}

// COMM_SET_CURRENT_BRAKE: firmware divides by 1000.0; positive values only.
static void vescSetBrake(float amps) {
    if (amps < 0.0f) amps = -amps;
    int32_t v = (int32_t)(amps * 1000.0f);
    uint8_t p[5] = { COMM_SET_CURRENT_BRAKE,
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v) };
    vescSend(p, 5);
}

// COMM_ALIVE: resets the VESC UART watchdog timer (must arrive < timeout, default 500 ms)
static void vescAlive() {
    const uint8_t p[1] = { COMM_ALIVE };
    vescSend(p, 1);
}

// Poll COMM_GET_VALUES; extract RPM from the verified byte offsets and convert
// to km/h.  Returns true on successful parse.
static bool vescPollSpeed(float* speed_kmh) {
    VESC.flush();  // discard any stale bytes before the request
    const uint8_t req[1] = { COMM_GET_VALUES };
    vescSend(req, 1);

    uint8_t buf[80];
    uint16_t len = vescRecv(buf, sizeof(buf));
    if (len < 27 || buf[0] != COMM_GET_VALUES) return false;

    // RPM is at payload bytes [23..26] — big-endian int32, scale 1.0
    // Verified against vedderb/bldc commands.c:
    //   buffer_append_float32(send_buffer, mc_interface_get_rpm(), 1e0, &ind)
    //   where buffer_append_float32 writes (int32_t)(val * scale) big-endian.
    int32_t erpm = ((int32_t)buf[23] << 24) | ((int32_t)buf[24] << 16)
                 | ((int32_t)buf[25] <<  8) | ((int32_t)buf[26]);
    float mech_rpm = (float)erpm / POLE_PAIRS;
    *speed_kmh = fabsf(mech_rpm) * WHEEL_CIRC_M * 60.0f / 1000.0f;
    return true;
}

// =============================================================================
// CST816S capacitive touch — minimal polling read.
//
// Register layout (verified against AN-CST816T-v1.pdf and koendv/cst816t):
//   0x01 gesture_id   0x02 finger_num
//   0x03 xpos_h       0x04 xpos_l
//   0x05 ypos_h       0x06 ypos_l
//
// I2C pattern: write register address, STOP, then read 6 bytes
// (matches endTransmission(true) + requestFrom used in koendv/cst816t).
//
// Auto-sleep: the CST816S enters deep sleep after ~2 s of inactivity unless
// REG_DIS_AUTOSLEEP (0xFE) = 0xFF is written during init.  Without this the
// touch controller stops responding after the first 2 seconds!
// =============================================================================
static void touchInit() {
    // Hardware reset per datasheet: LOW ≥ 5 ms, then HIGH ≥ 50 ms
    // The koendv/cst816t library uses LOW 10 ms + HIGH 100 ms — use same.
    pinMode(PIN_TP_INT, INPUT_PULLUP);
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, HIGH); delay(10);
    digitalWrite(PIN_TP_RST, LOW);  delay(10);
    digitalWrite(PIN_TP_RST, HIGH); delay(100);  // must be >= 50 ms

    Wire.begin(PIN_TP_SDA, PIN_TP_SCL, 400000);

    // Disable auto-sleep so the chip doesn't go dark after 2 s idle.
    // REG_DIS_AUTOSLEEP = 0xFE, value 0xFF = disabled
    // (koendv/cst816t does this for all chips except CST716)
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0xFE);   // REG_DIS_AUTOSLEEP
    Wire.write(0xFF);   // 0xFF = disable
    Wire.endTransmission(true);
    delay(10);
}

static bool touchRead(uint16_t* x, uint16_t* y) {
    // Point to register 0x01 (REG_GESTURE_ID)
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0x01);
    if (Wire.endTransmission(true) != 0) return false;  // STOP after write
    // Read 6 bytes: gesture, finger_num, xh, xl, yh, yl
    if (Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)6, (uint8_t)true) != 6)
        return false;
    uint8_t d[6];
    for (int i = 0; i < 6; i++) d[i] = Wire.read();
    if ((d[1] & 0x0F) == 0) return false;  // no finger down
    *x = ((uint16_t)(d[2] & 0x0F) << 8) | d[3];
    *y = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
    return true;
}

// =============================================================================
// Display helpers
// =============================================================================
static void drawScreen(float speed_kmh, const char* state, uint16_t state_color) {
    char buf[16];

    // State banner
    tft.fillRect(0, 0, 240, 32, state_color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, state_color);
    tft.setTextFont(2);
    tft.drawString(state, 120, 16);

    // Speed number (font 7 = 7-segment style)
    tft.fillRect(20, 85, 200, 70, TFT_BLACK);
    tft.setTextFont(7);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%4.1f", speed_kmh);
    tft.drawString(buf, 120, 120);

    tft.setTextFont(2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("km/h", 120, 162);

    // Hint footer
    tft.drawString("top=gas   bot=brake", 120, 218);
}

// =============================================================================
// setup / loop
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[POC] mcu_poc — scooter-gose-vrooom");

    // Display init
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.drawString("Booting...", 120, 120);

    // Touch init (includes auto-sleep disable)
    touchInit();
    Serial.println("[POC] touch OK");

    // VESC UART
    VESC.begin(115200, SERIAL_8N1, PIN_VESC_RX, PIN_VESC_TX);
    VESC.setTimeout(80);
    delay(100);

    // Ensure motor is stopped before we enter the control loop
    vescSetCurrent(0.0f);
    vescAlive();
    delay(100);

    tft.fillScreen(TFT_BLACK);
    Serial.println("[POC] ready  — touch screen to move");
}

void loop() {
    static uint32_t last_alive_ms = 0;
    uint32_t now = millis();

    // --- 1) Read touch -------------------------------------------------------
    uint16_t tx = 0, ty = 0;
    bool touched = touchRead(&tx, &ty);

    // --- 2) Decide state & send motor command --------------------------------
    // Speed enforcement: if we're already at/above the cap, stop applying gas
    // regardless of touch.  This is the MCU-side software limit.  The VESC's
    // l_max_erpm is the hardware-level backstop.
    const char*  state;
    uint16_t     state_color;

    if (touched && ty < 120 && g_speed_kmh < MAX_KMH) {
        // GAS — fixed current, VESC handles commutation
        vescSetCurrent(GAS_AMPS);
        state       = "GAS";
        state_color = TFT_GREEN;
    } else if (touched && ty >= 120) {
        // BRAKE — regenerative braking
        vescSetBrake(BRAKE_AMPS);
        state       = "BRAKE";
        state_color = TFT_RED;
    } else {
        // COAST — 0 A lets the motor freewheel; also acts as alive keep-alive
        vescSetCurrent(0.0f);
        state       = (g_speed_kmh >= MAX_KMH) ? "SPEED LIMIT" : "COAST";
        state_color = (g_speed_kmh >= MAX_KMH) ? TFT_ORANGE   : TFT_YELLOW;
    }

    // --- 3) Heartbeat (belt-and-suspenders; motor commands already reset it) --
    if (now - last_alive_ms >= 200) {
        vescAlive();
        last_alive_ms = now;
    }

    // --- 4) Poll telemetry every 200 ms --------------------------------------
    if (now - last_poll_ms >= 200) {
        float kmh = 0.0f;
        if (vescPollSpeed(&kmh)) {
            g_speed_kmh = kmh;
        }
        last_poll_ms = now;
    }

    // --- 5) Redraw every 100 ms ----------------------------------------------
    if (now - last_draw_ms >= 100) {
        drawScreen(g_speed_kmh, state, state_color);
        last_draw_ms = now;
    }

    delay(20);
}
