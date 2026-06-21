# Phase 0 Research: Multi-Stage Fluid Simulation Pendant

This document resolves every open technical question from the plan's Technical Context. For each
decision involving a possible algorithm change, the alternative is explained in full (per explicit
request) even where it was **not** adopted, so the tradeoff is visible.

## Decision 1 — Pressure-solver iteration count

**Decision**: Keep the existing solver (Gauss-Seidel sweep with Successive Over-Relaxation,
ω = 1.9) exactly as written in `flip_fluid.c`'s `solve_incompressibility`, but reduce
`numPressureIters` from the current desktop default of **100** down to a small tunable value
(start at **20**, verified on real hardware against visual stability per spec edge cases).

**Rationale**: This is a parameter change, not an algorithm change — it satisfies the spec's
"zero rewritten formulas" requirement (FR-003, SC-002) while being the single biggest lever for
fitting the 50ms/tick budget. The current 100-iteration default was tuned for a 120 FPS desktop
budget on an 8x8 interior cell grid; SOR with ω near 1.9 converges far faster than plain
Gauss-Seidel or Jacobi, and a 16x15 grid feeding a binary on/off LED display does not need
near-exact pressure convergence — visually-plausible incompressibility is enough. 20 iterations on
a 16x15 grid is ~5x fewer iteration-cells than the original 100 iterations on the original ~8x8
grid, and is the starting point for on-hardware tuning in tasks.md.

**What SOR/Gauss-Seidel is** (since this is being kept, but is worth explaining): each pressure
solver iteration sweeps every fluid cell and nudges its pressure toward the value that would make
local velocity divergence zero, using *already-updated* neighbor values from earlier in the same
sweep (that's "Gauss-Seidel" — as opposed to Jacobi, which would only use last-iteration's values).
SOR multiplies the correction by an over-relaxation factor (ω = 1.9, close to the stable limit of
2.0) to overshoot slightly and converge faster. Repeating this sweep `numPressureIters` times
gradually drives the velocity field toward divergence-free (incompressible).

**Alternatives considered**:
- *Red-black Gauss-Seidel* (checkerboard sweep ordering, splitting cells into two independent
  "colors" so each color's update doesn't depend on values computed earlier in the *same* sweep):
  this primarily speeds up *parallel* (multi-core/SIMD) solves, since it removes the in-sweep data
  dependency. On a single-core Cortex-M4 doing one sequential sweep, it does not meaningfully
  reduce wall-clock time over the existing ordering — not adopted, since it would also be a larger
  code change for little single-core benefit.
- *Switching to a different solver entirely (e.g., Conjugate Gradient)*: converges in fewer
  iterations for large grids, but has higher per-iteration overhead and code complexity; not
  worth it for a 16x15 grid where the existing solver already converges quickly once the
  iteration count is tuned down.

## Decision 2 — Numeric representation: float32, not double, not fixed-point

**Decision**: Use `float` exclusively throughout the physics core. Change `simulateFlipFluid`'s
`gravity_x`/`gravity_y` parameters from `double` to `float` (currently the only `double` usage in
the simulation core — everything else already uses `float` and `*f`-suffixed math like `sqrtf`,
`floorf`). Do **not** introduce fixed-point (Q-format) arithmetic.

**Rationale**: this directly resolves the constitution's previous (incorrect) mandate to prefer
fixed-point for a no-FPU chip. The real target, STM32L431CC, is a Cortex-M4F with a hardware
single-precision FPU. Confirmed via web research: the Cortex-M4 FPU implementation raises
floating-point performance by roughly an order of magnitude versus an FPU-less Cortex-M3, while
`double` on Cortex-M4F is software-emulated (slow) since the hardware FPU is single-precision
only. Fixed-point would only outperform float here via SIMD-packed 16-bit ops, which is not worth
the added complexity (rescaling, overflow tracking, loss of precision in the pressure solver) for
a budget that float32 already comfortably meets once iteration count is tuned (Decision 1).

**Alternatives considered**:
- *Q15/Q31 fixed-point via CMSIS-DSP*: rejected — no performance need once the chip is correctly
  identified as having an FPU; adds real implementation risk (overflow, rescaling bugs) for no
  benefit.
- *Keep `double`*: rejected — wastes cycles on software emulation for precision the simulation
  doesn't need (the FLIP solver already runs in `float` on both the JS prototype's numbers and
  the existing C code; only two parameters were ever `double`).

## Decision 3 — Grid and particle resolution: 1:1 with the 16x15 display

**Decision**: Resize the simulation domain so `fNumX` × `fNumY` (and the solver's `GRID_X`/`GRID_Y`
output constants, currently hardcoded to 10x10 in `flip_fluid.c`) map directly to the final
16-row x 15-column display — one simulation cell per LED, no separate downsampling step. Scale
the particle count proportionally using the same hexagonal-packing formula already in
`setupScene()` (`flip_utils.c`), just against the larger tank dimensions implied by the bigger
grid.

**Rationale**: spec FR-004 already commits the Windows simulator to the same 16x15 resolution as
the final hardware, specifically so it previews real device behavior. A 1:1 cell-to-LED mapping
needs zero new mapping logic — the existing `update_cell_colors_from_types` function already
writes directly into a `GRID_X`×`GRID_Y` buffer; only the constants change. Adding a separate
downsampling/aggregation step (e.g., averaging multiple sim cells into one LED) would itself be
new logic, which contradicts the spec's "zero rewritten formulas" requirement, for no visible
benefit at this display resolution.

**Alternatives considered**:
- *Keep the ~10x10 sim grid, nearest/area-sample down to 16x15 for display*: rejected — requires
  writing a new mapping function (new logic) and wastes the 16x15 display's actual addressable
  resolution.
- *Increase sim grid beyond 16x15 for higher internal fidelity, downsample to 16x15 for display*:
  rejected — pure cost with no visible benefit, since the LED matrix can only ever show 16x15
  values; would also need new downsampling logic.

## Decision 4 — Charlieplexed LED matrix driver pattern

**Decision**: Reuse and scale the existing TIM+DMA driver pattern from
`stm_projects/sample-charlieplexing`: DMA writes to `GPIOx->BSRR` (atomic set/reset of pin output
levels) and `GPIOx->MODER` (reconfigures a pin between push-pull output and Hi-Z/input, which is
what makes true charlieplexing possible — each pin must cycle through drive-high, drive-low, and
floating states) driven by a hardware timer, with zero CPU polling. Port this pattern from the
sample project (currently sized for a different pin count, on a different MCU package) onto the
axelor board's 16 GPIO pins allocated to the charlieplex bus.

**Rationale**: this already satisfies the constitution's DMA-driven-display and no-polling rules,
and is proven working on this project's own hardware family — reusing it is strictly lower-risk
than inventing a new driving scheme. The existing schematic (diode-matrix charlieplexing) confirms
the hardware expects exactly this drive-high/drive-low/Hi-Z cycling per pin pair.

**Alternatives considered**:
- *Bit-banged GPIO toggling in the main loop*: rejected outright — violates the constitution's
  explicit "polling loops are PROHIBITED" rule and would consume CPU budget needed for the
  physics tick.
- *External SPI/I2C LED driver IC (e.g., a constant-current shift-register chip)*: rejected — the
  existing schematic already wires the matrix as direct-GPIO charlieplexing via diodes, not
  through a driver chip; changing this would mean redesigning hardware that already exists.

## Decision 5 — Accelerometer read pattern (MPU-6500)

**Decision**: Read the **MPU-6500** over SPI1 (already configured in `axelor.ioc` at 625 kbit/s,
well within the part's documented 1 MHz "all registers" SPI limit) using an interrupt-driven
transfer, not polling. Configure the part into its **Low-Power Accelerometer Mode** (duty-cycled,
gyroscope off) rather than full 6-axis/Low-Noise mode, and drive each read from the part's **Data
Ready interrupt** on its `INT` pin, following the SPI-driver structure already established in this
repo's `stm_projects/adxl-362-sensor` and `STM32F103C8T6_projects/basic-adxl-with-matrix` (the
SPI/interrupt wiring pattern carries over; the register map and init sequence are MPU-6500-specific
and documented below). Disable the gyroscope axes (`PWR_MGMT_2` register, `DIS_XG`/`DIS_YG`/`DIS_ZG`
= 1) since this project has no use for rotation rate — only the X/Y accelerometer axes feed the
simulation's gravity vector. Apply a small dead-band/low-pass filter to the raw X/Y reading before
using it as the simulation's gravity vector, to satisfy the spec's edge case requirement that
sudden acceleration spikes or sensor noise must not destabilize the displayed fluid.

**What changed vs. the originally-assumed ADXL362**: the MPU-6500 (per its product specification,
InvenSense PS-MPU-6500A-01) is a 6-axis part (3-axis gyro + 3-axis accelerometer) rather than an
accelerometer-only part, communicates via a standard 4-wire SPI (`SCLK`/`SDI`/`SDO`/`nCS`, all
registers accessible at up to 1 MHz, sensor/interrupt registers only at up to 20 MHz — 1 MHz is
sufficient here), and outputs 16-bit two's-complement accelerometer data with a programmable
full-scale range (±2g/±4g/±8g/±16g). ±2g (16,384 LSB/g) is the recommended full-scale range for
this feature: it gives the finest resolution for a tilt sensor that should rarely see more than
~1g of net acceleration once dead-banded. Its dedicated **Low-Power Accelerometer Mode** publishes
documented current draw as a simple function of output data rate
(`Supply Current (µA) = 6.9 + ODR × 0.376`, e.g. ~18.7 µA at 31.25 Hz) — far below the 450 µA
full-rate accelerometer mode or 3.4 mA full 6-axis mode — and is the mode this project should use,
since the constitution requires minimizing power and the simulation does not need gyroscope data
or a high sample rate.

**Rationale**: matches the constitution's explicit "gravity input MUST be read via interrupt, not
polling" rule; the Low-Power Accelerometer Mode + Data Ready interrupt combination is the part's
own documented low-power pattern for exactly this use case (periodic accelerometer sampling without
a host-side polling loop), and reuses this project's already-proven SPI/interrupt driver structure
rather than inventing a new one.

**Alternatives considered**:
- *Continuous polling at a fixed rate in the main loop*: rejected — explicitly prohibited by the
  constitution.
- *MPU-6500's Wake-on-Motion interrupt mode* (interrupts the host only when acceleration exceeds a
  programmable threshold, intended for waking an applications processor from sleep): rejected for
  the main sampling path — this project wants regular small-tilt updates to drive a continuously
  animated fluid, not just a wake-up on a large shake. It remains a candidate for a future "idle/
  display-off" power state, but is out of scope for this feature.
- *Full 6-axis mode using both gyro and accelerometer (e.g., for a filtered orientation estimate)*:
  rejected — adds gyroscope power draw and DMP/sensor-fusion complexity for no benefit, since the
  spec only needs a 2D tilt/gravity-direction input, which the accelerometer alone provides.

## Decision 6 — Target MCU correction

**Decision/record**: The target MCU is **STM32L431CC** (Cortex-M4F, up to 80 MHz, 256 KB flash,
64 KB SRAM, hardware single-precision FPU), confirmed by the user via
https://www.st.com/en/microcontrollers-microprocessors/stm32l431cc.html, and matching
`stm_projects/axelor/axelor.ioc` (`Mcu.UserName=STM32L431CCTx`, `RCC.SYSCLKFreq_VALUE=80000000`,
`SPI1` already configured at 625 kbit/s — used for the MPU-6500 per Decision 5 above). This
corrects the constitution, which previously and incorrectly recorded STM32F103C8T6 (no FPU). The
constitution was amended to v2.0.0 as part of this planning pass (see
`.specify/memory/constitution.md`'s sync impact report) before this research was finalized, since
Decisions 1–2 above depend on which chip is actually being targeted. The constitution was amended
again to v2.1.0 later in the same session for the sensor part correction (ADXL362 → MPU-6500,
Decision 5).

## A different algorithm family considered and explicitly rejected (taught per request, not adopted)

**What it is**: *Stable Fluids* (Jos Stam, 1999) is a different, commonly-used real-time fluid
algorithm. Unlike the particle-based FLIP/PIC method already in this codebase, Stable Fluids is
purely grid-based (Eulerian): instead of tracking particles and transferring velocity to/from a
grid, it advects velocity directly on the grid using *semi-Lagrangian advection* — for each grid
cell, it traces backward along the velocity field to find where that fluid "came from" last
frame, then samples (interpolates) the velocity there. This is paired with the same kind of
pressure-projection step already in this codebase (to enforce incompressibility). It is popular
because semi-Lagrangian advection is *unconditionally stable* (it cannot blow up regardless of
time step), and it has no particle bookkeeping at all — no spatial hash, no particle-to-grid or
grid-to-particle transfer, no particle separation pass.

**Why it was not adopted here**: it would be lighter computationally (removing the particle
transfer and separation passes entirely), but it is a different algorithm, not a parameter change
to the existing one — adopting it would mean rewriting the physics formulas, which directly
contradicts the spec's explicit requirement (FR-003, FR-005, SC-002, SC-003) to reuse the existing,
already-working ported math unchanged across the Windows simulator and the firmware. It is
recorded here so it's available as a documented option if a future iteration ever needs to revisit
that requirement.

## Summary of resolved unknowns

| Unknown | Resolution |
|---|---|
| Target MCU (constitution said STM32F103C8T6; user said "STM32 L4") | STM32L431CC, confirmed by user-provided ST datasheet link + `axelor.ioc` |
| Numeric type for physics core | `float` only, no `double`, no fixed-point |
| Pressure solver iteration count for real-time budget | Reduce from 100 to ~20, tune on hardware; same SOR/Gauss-Seidel algorithm |
| Simulation grid resolution vs. display resolution | 1:1 — resize sim grid to 16x15 to match the LED matrix exactly |
| LED matrix drive mechanism | Reuse TIM+DMA GPIO `BSRR`/`MODER` pattern from `sample-charlieplexing`, scaled to 16 pins |
| Sensor part and read mechanism | **MPU-6500** (corrected from ADXL362), accelerometer axes only, gyro disabled; interrupt-driven SPI1 read in Low-Power Accelerometer Mode using the part's Data Ready interrupt |
