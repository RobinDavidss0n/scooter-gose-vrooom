#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>

// --- Correct 1.28" Microcontroller Hardware Pin Map ---
#define LCD_BL_PIN   2   // Direct MCU pin for Backlight PWM control
#define LCD_RST_PIN 14   // Direct MCU pin for Display Hardware Reset

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

// 2. LVGL Graphics Integration Framebuffer Hooks
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

LV_IMG_DECLARE(headlight);
LV_IMG_DECLARE(left_blinker);
LV_IMG_DECLARE(right_blinker);

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

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello Robin!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    setup_headlight_icon();
    setup_left_blinker_icon();
    setup_right_blinker_icon();
    
    Serial.println("Done with setup.");
}

void loop()
{
    lv_timer_handler();
    delay(5);

    // Logic here:
        // update scooter speed
        // update blinker icons left / right
        // update battery level
        // update headlight
        // update what mode the scooter is in
}