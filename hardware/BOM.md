# Bill of Materials

| # | Component | Part Number / Description | Qty | Est. Price |
|---|---|---|---|---|
| 1 | ESC | Makerbase VESC MINI 6.7 — 13S, heat sink, 50A continuous | 1 | ~$65 |
| 2 | DC-DC converter | XL7015 LM2596-based step-down, 5V–80V in, 5V–20V adj out | 1 | ~$3 |
| 3 | Display/MCU | Waveshare ESP32-S3 1.28″ Round Touch LCD (240×240, GC9A01, CST816S) | 1 | ~$20 |
| 4 | Connectors | XT30 male+female pair (battery to VESC) | 2 | ~$2 |
| 5 | Connectors | MR30 3-pin motor phase connectors | 1 set | ~$3 |
| 6 | Connectors | JST-PH 5-pin Hall sensor connector | 1 | ~$1 |
| 7 | Connectors | JST-XH 3-pin throttle connector | 1 | ~$1 |
| 8 | Wire | 12 AWG silicone wire, 1 m red + 1 m black (battery/VESC) | 1 set | ~$4 |
| 9 | Wire | 22 AWG stranded, assorted colours (signal wires) | 1 set | ~$3 |
| 10 | Heat shrink | Assorted sizes | 1 set | ~$2 |
| 11 | Capacitor | 63V 1000 µF electrolytic (battery side of VESC, optional but recommended) | 2 | ~$2 |
| 12 | Fuse + holder | 30A blade fuse + inline holder (main battery line) | 1 | ~$3 |
| 13 | Enclosure | 3D-printed or waterproof ABS box for electronics | 1 | ~$5–15 |

**Total estimate: ~$115–130 USD** (excl. tools, fasteners, printing)

## Tools Required

- Soldering iron + solder (60/40 or lead-free)
- Heat gun (heat shrink)
- Wire stripper / crimper
- Multimeter
- USB-C cable (for flashing ESP32-S3 and VESC via USB)
- PC with VESC Tool installed

## Notes

- The **XL7015 DC-DC** module needs to be **pre-adjusted to 5.0 V** (use a multimeter) before connecting to the ESP32-S3.
- The ESP32-S3 Waveshare board accepts 5 V on the `5V` or `VBUS` pin.
- The VESC MINI 6.7 has an onboard 5 V / 3.3 V output (useful for Hall sensor power) — confirm max current rating before drawing from it.
- For the original Xiaomi throttle (Hall type, 3-wire): connect to VESC ADC1 (signal), 3.3 V or 5 V (power), GND. Check VESC `app_adc_conf` for calibration.
