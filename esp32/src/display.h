#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

// ---------------------------------------------------------------------------
// Hardware display driver for the Waveshare 1.28" GC9A01 round LCD.
// Pin assignments are fixed for this board layout.
// ---------------------------------------------------------------------------
class LGFX_Waveshare_128 : public lgfx::LGFX_Device
{
    lgfx::Panel_GC9A01  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;

public:
    LGFX_Waveshare_128()
    {
        {
            auto cfg      = _bus_instance.config();
            cfg.spi_host  = SPI2_HOST;
            cfg.pin_sclk  = 10;
            cfg.pin_mosi  = 11;
            cfg.pin_miso  = 12;
            cfg.pin_dc    = 8;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg           = _panel_instance.config();
            cfg.panel_width    = 240;
            cfg.panel_height   = 240;
            cfg.pin_cs         = 9;
            cfg.pin_rst        = LCD_RST_PIN;
            cfg.invert         = true;   // GC9A01 polarity is inverted
            cfg.rgb_order      = true;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};
