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
