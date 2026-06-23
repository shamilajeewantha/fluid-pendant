# Contract: MPU-6500 Sensor Driver Module

**Source**: new `sensor/mpu6500_driver.c` in the Stage 3 firmware project. The SPI/interrupt
driver *structure* is adapted from this repo's existing `stm_projects/adxl-362-sensor` and
`STM32F103C8T6_projects/basic-adxl-with-matrix` projects; the register map, init sequence, and
power-mode configuration are specific to the MPU-6500 (research.md Decision 5), since the sensor
part was corrected from the originally-assumed ADXL362 to the MPU-6500 during planning.

## Contract

- Reads the MPU-6500 over SPI1 (already configured in `axelor.ioc`, 625 kbit/s — within the
  part's documented 1 MHz "all registers" SPI limit) using an interrupt-driven transfer — MUST
  NOT poll in a tight loop (constitution: gravity input read via interrupt, not polling).
- MUST configure the part into **Low-Power Accelerometer Mode** (duty-cycled; `PWR_MGMT_1.CYCLE`
  = 1) with the **gyroscope axes disabled** (`PWR_MGMT_2.DIS_XG/DIS_YG/DIS_ZG` = 1) — this project
  has no use for gyroscope data, and leaving it enabled costs power for nothing.
- MUST drive reads from the part's **Data Ready interrupt** on its `INT` pin (configured via
  `INT_ENABLE`), not its Wake-on-Motion interrupt — this project wants regular small-tilt updates,
  not a wake-up only on large motion.
- MUST use the ±2g full-scale range (`ACCEL_CONFIG.AFS_SEL` = 0, 16,384 LSB/g) — the finest
  resolution available, appropriate for a tilt sensor that should rarely see much more than 1g of
  net acceleration once dead-banded.
- Output: one `MotionInput` (`x`, `y` as `float`) per read, converting the 16-bit two's-complement
  `ACCEL_XOUT`/`ACCEL_YOUT` registers to `g`, then dead-banded/low-pass filtered inside this
  module before being handed to the physics core, so raw sensor noise never reaches
  `simulateFlipFluid` directly (spec edge case: sensor noise/spikes must not destabilize the
  display).
- On a missing or invalid sample, MUST return the last known-valid `MotionInput` rather than a
  zeroed or garbage value (spec edge case: momentary sensor dropout).
- MUST NOT reference `FlipFluid`/`Scene` internals or the display driver — it only produces
  `MotionInput` values.

## Consumers

- Stage 3's `main.c` glue: reads the latest `MotionInput` each tick and writes it into
  `Scene.gravity_x` / `Scene.gravity_y` before calling `simulateFlipFluid`.

## Verification

- On-hardware check: tilt the board, confirm `MotionInput` values track the tilt direction/
  magnitude as expected before wiring it into the full simulation loop.
- Confirm measured supply current in Low-Power Accelerometer Mode is in line with the part's
  documented `6.9 + ODR × 0.376` µA model (constitution: power consumption MUST be validated on
  real hardware, not assumed from the datasheet alone).

## Round 8 status (2026-06-22, research.md Decision 16)

The full contract above (interrupt-driven, Low-Power Accelerometer Mode, dead-banded/low-pass
filtered `MotionInput` output) is **not yet implemented**. What exists today in `axelor` is the
polled `MPU6500_ReadAccel()` read at the existing 500ms loop tick, now also printing the `m/s²`-
converted value (data-model.md `AccelTelemetry`) for on-hardware verification — see
quickstart.md's Round 8 section. This is a UART-observable checkpoint on the way to this contract,
not a fulfillment of it; the interrupt-driven/low-power/filtered requirements above remain open
work for whenever physics-core integration begins.

## Round 9 status (2026-06-23, research.md Decision 22) — reconciled against as-built code

Physics-core integration is now complete (see `contracts/physics-core.md`'s Round 9 status), and
the polled read now genuinely drives the simulation — but the read mechanism itself is **still
polling**, not interrupt-driven, and **still not** Low-Power Accelerometer Mode; this is a logged,
justified, pre-existing constitution deviation (`plan.md`'s Complexity Tracking table), not new
non-compliance introduced this round. What changed in `main.c`: `accelX_mps2`/`accelY_mps2`
(promoted from `MPU6500_ReadAccel()`'s local variables to file-scope globals, so the value survives
past the function's return) now feed `simulateFlipFluid` as `gravity_x`/`gravity_y`, clamped first
via a small `ClampGravityMps2()` helper to `±19.62 m/s²` (spec edge case: shake stability) — this
clamp is the one piece of this contract's "Output" requirement (dead-banded/low-pass filtered) that
exists today; full filtering, missing-sample hold-last-value behavior, and the interrupt-driven/
low-power rework all remain open, deferred work.

`MPU6500_ReadAccel()` was also moved to the top of the main loop (previously called last) so each
tick's freshest sample drives that same tick's simulation step rather than the previous tick's, and
its own UART prints (`raw=.../mm/s2=...`) are now throttled to roughly every 25th call (~500ms at
the new 20ms loop period) via a shared `printTick` counter, matching the loop's other throttled
prints (research.md Decision 21) — purely a print-volume change, no effect on the read itself or
on what reaches the simulation.

## Round 10 status (2026-06-23, research.md Round 10) — accelerometer axes swapped relative to display

On-hardware testing of the real simulation found the lit fluid settling toward the display's right
edge (a column/horizontal position) when the pendant was held upright in its normal worn
orientation, instead of the bottom row (vertical) as expected. Rotating the pendant 90° made it
settle correctly, confirming this was a pure axis swap, not a sign/mirroring issue: the MPU-6500's
physically-mounted X axis is actually the board's vertical axis, and its Y axis is the board's
horizontal axis — the reverse of what `main.c` originally assumed when it fed `accelX_mps2` into
`gravity_x` (horizontal, per `flip_fluid.c`'s `dstY*GRID_X+dstX` cell-color convention) and
`accelY_mps2` into `gravity_y` (vertical). Fixed by swapping the two assignments in `main.c`'s main
loop: `clampedGravityX` now reads `accelY_mps2` and `clampedGravityY` now reads `accelX_mps2`. This
is a board-specific glue-code fix only — `MPU6500_ReadAccel()` itself, the raw/mg values it reports,
and `simulateFlipFluid`'s own `gravity_x`/`gravity_y` parameter semantics are unchanged. Rebuilt and
build-verified: 0 errors, 0 warnings, both Debug and Release configs.
