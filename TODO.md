# TODO

Project was on ice since 2026-06-14. Status recap and next steps below.

## Wiring (do in order)

Physical split: MCU + front light in the **handlebar**; VESC + battery + rear
light in the **body**. Routing finalized — see
[wiring/diagram.mmd](wiring/diagram.mmd) v5 for the full picture.

- [ ] Run 1 new 0.75mm² wire (common GND) alongside the OG cable on its
      existing external path past the fold hinge; heat-shrink bundle in
      segments (not across the hinge) to preserve the OG cable's slack loop.
- [ ] Repurpose all 5 OG conductors: UART TX, UART RX, rear-light brake-signal
      GPIO, 5V+, and 12V+ (no spares left).
- [ ] Wire MCU 5V+GND+UART directly to the VESC COMM port (stop feeding MCU
      from the step-down).
- [ ] Set the step-down (XL7015) trimpot to output 12V directly (adjustable —
      no step-up board needed); verify with a multimeter before connecting
      either light.
- [ ] Solder the front-light MOSFET **in the handlebar** (local GPIO, no pole
      wire needed): gate -> 100Ω -> MCU GPIO, 10kΩ pulldown to handlebar
      ground, drain -> front light `-`, source -> handlebar ground.
- [ ] Wire the rear-light MOSFET **in the body**, gate driven by the new
      brake-signal wire instead of a local GPIO.
- [ ] Wire rear light `+` locally to step-down `12V OUT` in the body; solder
      the 0.75mm² XT60 36V feed into step-down `IN+`/`IN-`.
- [ ] Bench-test lighting ~5 min, watch XL7015 temperature; then heat-shrink +
      hot-glue/silicone strain-relief the step-down board (no thermal paste).
- [ ] Once WiFi/BT is added to the MCU: add a local buffer cap (470-1000µF
      low-ESR + 100nF ceramic) at the MCU's 5V input in the handlebar,
      referenced to `HB_GND`, to absorb TX burst current spikes instead of
      pulling them through the long OG4/GND wires.

See [wiring/current-progress-notes.md](wiring/current-progress-notes.md) and
[wiring/diagram.mmd](wiring/diagram.mmd) for full details.

## Firmware

- [ ] Add GPIO output control for the two lights (headlight/rear) — pins, a
      simple on/off (or blink for turn signals) function, and hook into
      `ui.cpp`/`main.cpp`. Icons already exist in `esp32/src/images/` but are
      not wired to any GPIO yet.
- [ ] Decide whether touch input (CST816S) is in scope, or drop it from the
      README hardware table if unused.
- [ ] Re-verify the firmware still builds after the time off
      (`/home/rdw/.platformio/penv/bin/platformio run`) before touching
      hardware again.

## Housekeeping

- [ ] Decide on committing the `wiring/` folder reorg (currently uncommitted:
      deleted `wiring-help-notes.txt`, new `current-progress-notes.md` +
      `diagram.mmd`).
- [ ] Once lighting wiring is done, do a full bench test of the scooter with
      wheel lifted (per `testing/poc/README.md` safety approach) before a
      real ride.
