# TODO

Status recap as of 2026-08-26. Firmware (lights, blinkers/speed-adjust) is
done and builds clean; physical wiring is the remaining work.

## Wiring (do in order)

Physical split: MCU + front light in the **handlebar**; VESC + battery + rear
light in the **body**. Single-stage 36V->12V via the XL7015 (no step-up
board). Full wire-color legend, MOSFET pin colors (Gate=White, Drain=Grey,
Source=Black), and layout are in [wiring/diagram.mmd](wiring/diagram.mmd) —
render `wiring/diagram.dist.png` after any change (see
`/memories/repo/wiring.md` for the render command).

- [x] Chassis wiring done: battery, XT60, VESC, XL7015, rear light all
      connected. XL7015 trimpot verified at 12V with a multimeter before any
      light was connected; VESC powers up and lights normally.
- [ ] Run the new cable (`N1` common GND, plus `N2`/`N3` for the soft power
      switch below — 3 conductors now) alongside the OG cable on its existing
      external path past the fold hinge; heat-shrink bundle in segments (not
      across the hinge) to preserve the OG cable's slack loop.
- [ ] Before soldering the MCU's 12-wire ribbon: tag/label the `VSYS` and
      `3V3` conductors — both are Red on the physical ribbon, and swapping
      them feeds 5V into the 3.3V-only MCP23017 (abs max ~5.5V).
- [ ] Solder the rear-light MOSFET's remaining gate protection: 100Ω series
      resistor (between `OG3` and the gate) and 10kΩ pull-down (gate to
      `CG`) — added to the diagram in an audit but not yet physically done.
- [ ] Build the two blinker MOSFET driver circuits (low-side IRLB8721,
      12V-fed, same pattern as front/rear — NOT the original 5V/transistor
      design, since the blinkers turned out to be full 12V LED modules).
- [ ] Wire the MCP23017 I2C expander (turn-signal buttons + blinker outputs)
      per `diagram.mmd`'s `EXPANDER`/`MCU` subgraphs, using the native `3V3`
      pin (no separate LDO needed).
- [ ] Build the soft power switch: JD1912 relay (12V coil, 40A SPST-NO,
      built-in flyback diode) gating the VESC only, driven by a small NPN
      latch transistor (e.g. BC337) + 2x 1N4148 diode-OR + 1kΩ base resistor
      — see `PWR_RELAY`/`PWR_LATCH` in `diagram.mmd`. Can be assembled at the
      chassis end any time, doesn't need to happen before closing the stem.
- [ ] Wire the handlebar power button to local 12V and to the new `N2`
      conductor (bootstraps the relay from fully off, no MCU involvement),
      plus the 10kΩ/3.3kΩ sense divider into `MCP_GPB1` so the MCU can detect
      a held press (2s) to trigger a graceful shutdown — see `diagram.mmd`.
- [ ] Bench-test all 4 lights (front, rear, both blinkers) together, watch
      XL7015 temperature — real headroom is ~7W recommended vs ~3.6-4W
      worst-case load, so it should run cool this time.
- [ ] Heat-shrink + hot-glue/silicone strain-relief the step-down board (no
      thermal paste).
- [ ] Once WiFi/BT is added to the MCU: add a local buffer cap (470-1000µF
      low-ESR + 100nF ceramic) at the MCU's 5V input in the handlebar,
      referenced to `HB_GND`, to absorb TX burst current spikes instead of
      pulling them through the long OG4/GND wires.

See [wiring/current-progress-notes.md](wiring/current-progress-notes.md) and
[wiring/diagram.mmd](wiring/diagram.mmd) for full details.

## Firmware

- [x] GPIO/PWM output control for both lights, plus MCP23017-driven blinkers
      and speed-limit adjust buttons (`esp32/src/lights.*`,
      `esp32/src/blinkers.*`) — builds clean via
      `/home/rdw/.platformio/penv/bin/platformio run` (RAM 23.6%, Flash
      17.8%).
- [x] Soft power switch hold/release + button sense (`esp32/src/power.*`) —
      asserts the relay hold line (`MCP_GPB0`) on boot; `power_poll()` reads
      the button's local sense divider (`MCP_GPB1`) and triggers a graceful
      shutdown after a 2s hold; `power off` console command does the same
      immediately. Bootstrap (first power-on) is a pure hardware path through
      the handlebar button, no firmware involvement.
- [ ] Decide whether touch input (CST816S) is in scope, or drop it from the
      README hardware table if unused — still no driver code for it.

## Housekeeping

- [ ] Once lighting wiring is done, do a full bench test of the scooter with
      wheel lifted (per `testing/poc/README.md` safety approach) before a
      real ride.
