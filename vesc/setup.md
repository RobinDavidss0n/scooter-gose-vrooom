# VESC Setup

Setup the ESC using the [VESC tool](https://vesc-project.com/vesc_tool).

## Auto Wizard Setup (Setup Motors FOC)

1. Chose Generic
2. Chose DD hub motor
3. Battery:
   - Type: Li-ion
   - Cells: 10
   - Capacity: 10 Ah <br>
  Advanced:
      - Regen 0 A (disabled for now until the project supports it safely)
      - Current max 19 A (roughly 700 W at 36V)
4. Setup:
   - Direct drive = checked
   - Wheel diameter: 254 mm (10 inch)
   - Motor poles: 30
   - Temperature sensor: NTC 10k at 25C
   - Beta value for motor thermistor: 3380K