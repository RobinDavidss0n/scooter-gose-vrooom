# VESC Starter

The repo now keeps two different things:

- `config/vesc_mcconf.xml` and `config/vesc_appconf.xml` are full exports from a working controller. Keep them as backups.
- `config/motor_config_xiaomi_elite_ne.xml` and `config/app_config.xml` are small reusable overlays. They are meant for a new motor on the same scooter platform after detection has already been run.

VESC Tool applies only the tags present in a partial XML file.

## Which workflow to use

If you are restoring the same known-good setup:

1. Read the live config from the VESC.
2. Load `config/vesc_mcconf.xml`.
3. Load `config/vesc_appconf.xml`.
4. Write both configs.

If you are fitting a new motor:

1. Read the live config from the VESC.
2. Run the FOC wizard.
3. Run hall detection if hall sensors are connected.
4. Verify direction and that the wheel spins cleanly.
5. Load `config/motor_config_xiaomi_elite_ne.xml`.
6. Load `config/app_config.xml` if you want the same UART and timeout behavior.
7. Write the configs and test with the wheel off the ground.

## What should be entered in the wizard

These are the scooter-level values that make sense to keep when the motor changes:

- Motor control: FOC.
- Sensor mode: Hall sensors, if the motor has them wired.
- Motor poles: 30.
- Gear ratio: 1.0.
- Wheel diameter: 0.254 m.
- Battery type: Li-ion.
- Battery cells: 10.
- Battery capacity: 10 Ah.
- Motor no-load current: 1 A.

## What should not be reused on a different motor

Do not carry these values over from one motor to another without rerunning detection:

- `foc_motor_r`
- `foc_motor_l`
- `foc_motor_ld_lq_diff`
- `foc_motor_flux_linkage`
- `foc_current_kp`
- `foc_current_ki`
- `foc_observer_gain`
- `foc_hall_table__*`
- `foc_offsets_*`

Those values are exactly why the wizard exists.

## What the small files keep

- `motor_config_xiaomi_elite_ne.xml`: current limits, battery cutoff, temperature limits, and wheel or battery metadata.
- `app_config.xml`: the minimum controller-side app settings that are independent of the motor, such as UART mode, baud rate, controller ID, and timeout behavior.
