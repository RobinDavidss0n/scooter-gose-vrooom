# Pin Reference

## Waveshare ESP32-S3 1.28″ Round Touch LCD

### On-board peripherals (fixed, no wiring needed)

| Signal | GPIO | Notes |
|---|---|---|
| LCD MOSI (SPI) | 11 | GC9A01 display |
| LCD SCLK (SPI) | 10 | GC9A01 display |
| LCD CS | 9 | GC9A01 chip select |
| LCD DC | 8 | GC9A01 data/command |
| LCD RST | 12 | GC9A01 reset |
| LCD BL | 40 | Backlight PWM (HIGH = on) |
| Touch SDA (I2C) | 6 | CST816S + QMI8658 shared |
| Touch SCL (I2C) | 7 | CST816S + QMI8658 shared |
| Touch INT | 4 | CST816S interrupt |
| Touch RST | 5 | CST816S reset |
| IMU INT1 | 3 | QMI8658 interrupt 1 |
| IMU INT2 | 2 | QMI8658 interrupt 2 |
| Charge indicator | 1 | LOW = charging |

### External connections (user-assigned)

| Signal | GPIO | Direction | Connected to |
|---|---|---|---|
| VESC UART TX | 17 | OUT | VESC RX |
| VESC UART RX | 18 | IN | VESC TX |
| Power 5V | 5V pin | IN | XL7015 OUT+ |
| GND | GND pin | — | XL7015 OUT- / VESC GND |

### Free GPIOs (available for expansion)

`14, 15, 16, 19, 20, 21, 43 (USB TX), 44 (USB RX)`

> GPIO 43/44 are used by the USB-CDC serial monitor — do not use while monitoring.

## VESC MINI 6.7 Pinout Reference

| Label | Description |
|---|---|
| B+ / B- | Battery positive / negative |
| A / B / C | Motor phase outputs |
| HA / HB / HC | Hall sensor inputs A/B/C |
| H5V | Hall sensor 5V supply |
| HGND | Hall sensor GND |
| ADC1 | Analog throttle input (0–3.3 V) |
| ADC2 | Analog brake input (optional) |
| TX / RX | UART (3.3 V logic) |
| CANH / CANL | CAN bus (future expansion) |
| 3V3 | 3.3 V output (limited current) |
| 5V | 5 V output (limited current) |
| GND | Ground |
