#include "display_manager.h"
#include "config.h"

// ==============================================================================
void DisplayManager::begin() {
    // --- TFT -----------------------------------------------------------------
    _tft.begin();
    _tft.setRotation(0);
    _tft.fillScreen(TFT_BLACK);

    // Backlight via PWM — ledc channel 0, 5 kHz, 8-bit resolution
    ledcSetup(0, 5000, 8);
    ledcAttachPin(LCD_BL, 0);
    ledcWrite(0, 255);   // full brightness on boot

    // --- Touch ---------------------------------------------------------------
    _touch.begin(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);

    // --- LVGL ----------------------------------------------------------------
    lv_init();

    // Allocate draw buffers from PSRAM if available, otherwise internal RAM
    size_t buf_px = DISPLAY_WIDTH * LV_BUF_LINES;
    if (psramFound()) {
        _buf1 = (lv_color_t*)ps_malloc(buf_px * sizeof(lv_color_t));
        _buf2 = (lv_color_t*)ps_malloc(buf_px * sizeof(lv_color_t));
        if (_buf1 && _buf2) {
            Serial.println("[DISP] LVGL buffers allocated in PSRAM");
        } else {
            // Partial alloc — free and fall through to static SRAM
            free(_buf1);  _buf1 = nullptr;
            free(_buf2);  _buf2 = nullptr;
            Serial.println("[DISP] PSRAM alloc failed, falling back to SRAM");
        }
    }
    if (!_buf1) {   // no PSRAM or alloc failed
        static lv_color_t static_buf1[DISPLAY_WIDTH * LV_BUF_LINES];
        static lv_color_t static_buf2[DISPLAY_WIDTH * LV_BUF_LINES];
        _buf1 = static_buf1;
        _buf2 = static_buf2;
        Serial.println("[DISP] LVGL buffers allocated in SRAM");
    }

    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, buf_px);

    // Register display driver
    lv_disp_drv_init(&_disp_drv);
    _disp_drv.hor_res    = DISPLAY_WIDTH;
    _disp_drv.ver_res    = DISPLAY_HEIGHT;
    _disp_drv.flush_cb   = lvFlushCb;
    _disp_drv.draw_buf   = &_draw_buf;
    _disp_drv.user_data  = this;
    lv_disp_drv_register(&_disp_drv);

    // Register input (touch) driver
    lv_indev_drv_init(&_indev_drv);
    _indev_drv.type      = LV_INDEV_TYPE_POINTER;
    _indev_drv.read_cb   = lvTouchCb;
    _indev_drv.user_data = this;
    lv_indev_drv_register(&_indev_drv);

    Serial.println("[DISP] Display manager ready");
}

// ------------------------------------------------------------------------------
void DisplayManager::update() {
    lv_timer_handler();
}

// ------------------------------------------------------------------------------
void DisplayManager::setBrightness(uint8_t val) {
    // ledcWrite(channel, duty): 0 = off, 255 = full brightness
    ledcWrite(0, val);
}

// ==============================================================================
// LVGL callbacks
// ==============================================================================

void DisplayManager::lvFlushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    auto* self = static_cast<DisplayManager*>(drv->user_data);
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    self->_tft.startWrite();
    self->_tft.setAddrWindow(area->x1, area->y1, w, h);
    self->_tft.pushColors(reinterpret_cast<uint16_t*>(color_p), w * h, true);
    self->_tft.endWrite();

    lv_disp_flush_ready(drv);
}

// ------------------------------------------------------------------------------
void DisplayManager::lvTouchCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    auto* self = static_cast<DisplayManager*>(drv->user_data);
    TouchPoint tp = self->_touch.read();

    if (tp.touched) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
