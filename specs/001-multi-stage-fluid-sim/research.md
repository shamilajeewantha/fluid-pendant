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

## Round 2 — Post-implementation visual QA fixes (2026-06-21)

After Stage 1/Stage 2 were built per Decisions 1–3 above, the user ran both and found four
issues. Each is resolved below as a further **parameter/UI-only** change — none rewrites the
FLIP/PIC formulas, consistent with the same constraint Decisions 1–3 operated under.

### Decision 7 — Border LEDs were structurally dead; pad the solver grid

**Problem**: every display cell along the left, right, and bottom edges never lit up, in both
Stage 1 and Stage 2. Root cause: `setupScene()`'s wall setup (`if (i==0 || i==fNumX-1 || j==0)
s=0.0`) marks those exact border cells permanently `SOLID`, never `FLUID_CELL` — and since
`fNumX`/`fNumY` were resized 1:1 with the display (Decision 3), those solid border cells *are*
the outermost ring of LEDs. They are not "rarely lit" — they are structurally incapable of ever
lighting. On the original pre-port code (10x10 solver) this was never visible because the old
display only ever showed a *cropped inner 8x8* of that 10x10 grid — the crop quietly hid the
dead wall ring. Decision 3's "1:1, no cropping" simplification removed that crop and exposed the
problem.

**Decision**: reintroduce a crop — but unlike the old crop (which Decision 3 rightly objected to
as it wasted the display's actual resolution), size the *solver's own grid* one cell larger than
the display on every side that has a wall, then map only the interior (non-wall) cells onto the
full `GRID_X`×`GRID_Y`/`DISPLAY_COLS`×`DISPLAY_ROWS` display:
- Columns (`fNumX`): walls exist on **both** sides (`i==0` and `i==fNumX-1`), so pad symmetrically:
  solver `fNumX = GRID_X + 2`.
- Rows (`fNumY`): a wall exists only at the **bottom** (`j==0`); the top (`j==fNumY-1`) is already
  open/fluid-eligible with no wall. Pad asymmetrically, bottom only: solver `fNumY = GRID_Y + 1`.
- The display-facing `cellColor` buffer stays exactly `GRID_X * GRID_Y` (240) — the contract in
  `contracts/physics-core.md` is unchanged. Only the *internal* solver resolution grows; the crop
  offset (`+1` on x, and a flip so solver row `fNumY-1` lands on display row 0) is applied where
  the existing crop/flip logic already lived (`update_cell_colors_from_types` in `flip_fluid.c`
  for Stage 2; `script.js`'s `getGridColors`/`setupScene` for Stage 1 — `flip.js` itself stays
  resolution-agnostic, unchanged, per the T011 finding).

**Why top isn't padded**: the gravity-direction control only ranges -90°..90° (`gravity_y =
-cos(angle)*9.81`, and `cos` is non-negative over that whole range), so simulated gravity *never*
points upward — fluid is never pulled toward the open top edge by design of this control, only by
splash/momentum. The top display row being sparse is physically correct fluid behavior under a
downward-biased gravity control, not a structural defect like the other three walled sides, so it
does not need the same fix.

**Alternatives considered**:
- *Make the border cells FLUID instead of SOLID*: rejected — a FLIP solver needs solid boundary
  cells to contain the fluid; without them, particles have no domain to push against and the
  simulation domain effectively has no edges, breaking pressure projection.
- *Leave it as a known limitation*: rejected — the LED matrix is physical hardware with a fixed
  pin count; leaving ~38 of 240 LEDs (the left/right/bottom ring) permanently unlit wastes a third
  of the addressable column range and a meaningful fraction of total LEDs for no benefit.

### Decision 8 — Stage 1/Stage 2 felt like different fluids; match the one differing parameter

**Problem**: the user observed Stage 2 (C) feeling dense/cohesive while Stage 1 (JS) felt
"lighter than water." Comparing every scene default between `scene.c`'s `scene_default()` and
`script.js`'s `scene` object turned up exactly **one** difference: `numPressureIters` — lowered to
`20` in Stage 2 by Decision 1 (for the embedded performance budget), but left at the original
`100` in Stage 1 (Decision 1 only targeted the shared physics core, not the independent JS
prototype, since Stage 1 has no embedded performance constraint). More pressure-solver iterations
converge the velocity field closer to fully divergence-free each tick, so it disperses/responds to
pressure gradients more readily (reads as "thinner/lighter"); fewer iterations leave more residual
divergence, so fluid clumps/resists spreading more (reads as "denser"). This is a side effect of
Decision 1 being applied asymmetrically, not a pre-existing issue.

**Decision**: lower Stage 1's `numPressureIters` from `100` to `20` in `script.js`, matching Stage
2. This is the same parameter Decision 1 already established as safe to tune (no formula
rewritten), now applied for visual-consistency reasons rather than a performance budget. Per
spec.md's Edge Cases section, Stage 1 vs. Stage 2/3 divergence is explicitly *allowed*, not
required to match — this fix is a quality improvement, not a contract obligation.

**Alternatives considered**: re-tuning `flipRatio`/`overRelaxation` instead — rejected, since
those were already identical between both stages; the iteration-count gap was the only actual
difference found, so it's the only change made.

### Decision 9 — Display polish: cell separators, row/column labels, smaller on-screen cells

**Problem**: Stage 2's window has no visible line between cells (Stage 1 already has this via its
CSS `gap: 2px`) and the on-screen cell size makes the window fill most of the screen; neither
stage labels row/column indices.

**Decision**: rendering-only changes, no simulation-scale values touched:
- Stage 2 (`DrawGrid` in `util.c`): outline each cell rectangle (e.g. `FrameRect`/a border brush)
  in addition to the existing fill, and reduce `CELL_SIZE` (currently 50px) to a smaller on-screen
  pixel size. `CELL_SIZE` is purely a Win32 drawing constant (pixels per cell on screen) — it has
  no relationship to `tankWidth`/`tankHeight`/`h`/`r` (the actual simulation-space scale), so
  changing it cannot affect simulation behavior.
- Both stages: draw row indices (0..15) along the left margin and column indices (0..14) along
  the top margin — static text (`TextOut`/`DrawText` for Stage 2; a labeled row/column of `<div>`s
  outside the `.grid` for Stage 1), not part of the simulated grid itself.

**Alternatives considered**: shrinking `CELL_SIZE` *and* rescaling the simulation tank together —
rejected per explicit user instruction ("don't touch any other scaling stuff... otherwise we will
[mess up] the simulation") — these are kept strictly independent: `CELL_SIZE`/CSS pixel sizes are
display-only, `tankWidth`/`tankHeight`/`h`/`r`/`resX`/`resY` (Decision 3/7) are simulation-only.

### Decision 10 — Stage 1 Start/Pause control: button instead of keyboard-only

**Problem**: Stage 1's only way to start/pause is pressing the `p` key (`script.js`'s
`keydown` handler) with no visible affordance in the UI for it, unlike Stage 2 which already has
explicit Start/Pause buttons (`hwndStartButton`/`hwndPauseButton` in `util.c`).

**Decision**: add explicit "Start"/"Pause" `<button>` elements to `flip_sim.html`, wired in
`script.js` to toggle `scene.paused` directly — matching Stage 2's existing UI pattern. The
keyboard shortcut can stay as a bonus shortcut, but the button becomes the primary, discoverable
control.

**Alternatives considered**: removing keyboard control entirely — not necessary; the gap was the
*missing visible button*, not the keyboard shortcut's existence.

## Round 3 — Matching both stages to real-water playback speed (2026-06-21)

After Round 2's fixes, the user asked why the two stages still "feel" like different fluids
(Stage 2 dense, Stage 1 light) and asked to "match them both to real water," explicitly
constrained to stay STM32L4-friendly ("not too much calculations... just enough").

### Decision 11 — Stage 2's physics clock didn't match its own tick rate

**Problem**: `scene_default()` sets `dt = 1.0f / 120.0f` (≈8.33ms of *simulated* time per physics
step), but `main.c` only calls `SimulateFluid` every `TIMER_INTERVAL = 20` ms of *real* time (a
`WM_TIMER` at 50 Hz). So every tick advances the fluid by 8.33ms while 20ms of wall-clock time
actually passes — Stage 2 plays its own physics back at only ~42% of real speed, i.e. in slow
motion. Stage 1's `setInterval(update, 1000/120)` targets the same ~8.33ms both for its tick
interval *and* its `dt`, so it already runs close to real speed by construction. Slow-motion fluid
reads as thick/syrupy; real-speed fluid reads as thin/watery — this alone explains the "different
fluid" perception, independent of the `numPressureIters` mismatch already fixed in Round 2
(Decision 8).

**Decision**: change Stage 2's `scene_default()` to set `dt = TIMER_INTERVAL / 1000.0f` (`0.02f`,
matching the existing 20ms timer interval) instead of the inherited `1.0f / 120.0f`. This makes
Stage 2 play back at real-world speed — water-like, not slow-motion — using the exact same number
of physics ticks per second as before (still 50 Hz). **This is the STM32L4-friendly choice**: the
alternative (lowering `TIMER_INTERVAL` to ~8ms to match the existing `dt`) would instead increase
the tick *rate* to 120 Hz — 2.4x more pressure-solver work per second, which is the wrong direction
for an MCU budget. Changing one constant costs nothing extra to compute.

**Verified empirically** (temporary headless harness, mingw-compiled, run directly, then deleted —
same pattern as Round 1/2's smoke tests): 3000 steps at `dt = 0.02f` with the existing
`numPressureIters = 20` produced zero NaN/Inf and no particles escaping the tank bounds — the
larger timestep does not destabilize the already-reduced iteration count, so no further iteration
increase (i.e., no extra compute) is needed.

**Stage 1 (JS)**: left unchanged. Its tick-interval target and `dt` were already both ~8.33ms, so
it has no equivalent mismatch to fix; this round's fix only applies to Stage 2.

**Other "real water" parameters considered and left unchanged**: gravity (`9.81 m/s²`) is already
real-world Earth gravity. `flipRatio = 0.9` (favors FLIP over PIC) is already the value Müller's
own reference demo uses specifically because real water has very low viscosity and FLIP preserves
more energetic, less-damped motion than PIC — lowering it would make the fluid look *more*
syrupy/viscous, the opposite of "real water." Neither needed any change.

**Alternatives considered**:
- *Accumulator/delta-time pattern* (measure actual elapsed wall-clock time each tick and step
  physics by that exact amount, possibly sub-stepping for large frames): more robust against
  Windows timer jitter, but is meaningfully more code/branching for a desktop preview tool whose
  job is just to approximate the eventual firmware's fixed-rate tick — rejected as more
  calculation than "just enough" for this stage; Stage 3's actual tick rate (once chosen) should
  just get its own matching fixed `dt`, the same lesson this fix teaches.
- *Lower `TIMER_INTERVAL` to ~8ms instead*: rejected — 2.4x more compute per second for the same
  visual result works against the explicit STM32L4 budget constraint.

## Round 4 — Surface-cell flicker at rest (2026-06-21)

The user reported that with the gravity slider held still, LEDs along the air/water boundary
kept blinking even after the fluid visually settled, and asked for the most robust, lightweight,
STM32L4-friendly fix (verified against literature, see Decision 12 sources).

### Decision 12 — Hysteresis threshold on the already-computed particle density, not the binary cellType

> **Superseded by Round 5's Decision 13** (below): rendering the continuous `particleDensity`
> value directly as brightness removes the binary on/off state this hysteresis logic was
> protecting, so the hysteresis/threshold code itself was removed in Round 5, not kept alongside
> it. Left here for the reasoning trail (why hysteresis was tried first, and what its own
> empirical testing revealed) — see Decision 13 for what actually shipped.

**Problem**: both `update_cell_colors_from_types` (C) and `updateCellColors` (JS) light an LED
purely from `cellType == FLUID_CELL` — a hard binary flag set by which single cell a particle's
rounded position currently falls in (`flip_fluid.c` line ~260: `xi = floor(x * h1)`, no smoothing).
Even at rest, particles never perfectly stop (continuous small corrections from the pressure
solver and particle-separation pass), so a particle hovering near a cell boundary flips that one
cell's binary flag every frame — a step function has all its sensitivity concentrated at one
exact point, so a sub-pixel jitter there produces a full 0→1→0 swing every tick. This is a
well-documented FLIP/PIC surface-noise artifact (Sources below), worse here because the grid is
coarse (15x16) with few particles per boundary cell.

**Decision**: replace the binary check with a **hysteresis threshold on `particleDensity`**
(C: `g_particleDensity`; JS: `this.particleDensity`) — a value **already computed every tick** by
both solvers (bilinear P2G splat, used today only for the pressure solver's drift compensation),
so reading it for display costs zero new physics computation. Add one small persisted
per-display-cell array (`ledState`, 240 cells) that only changes when density crosses one of two
thresholds, normalized against `particleRestDensity` (also already computed):
- Turn a cell **ON** once `density > 0.5 * restDensity`.
- Turn it **OFF** only once `density < 0.2 * restDensity`.
- Otherwise, leave `ledState` exactly as it was last frame.

This is the standard fix for exactly this class of problem — a Schmitt-trigger-style dual
threshold so a value oscillating near one boundary can't flip the output, confirmed against
established sources (below) rather than invented from scratch.

**Why this is the lightest correct fix and STM32L4-appropriate**: no new per-frame computation
beyond reading an existing array and two float comparisons per display cell (240 cells × 2
comparisons = 480 comparisons/tick — negligible next to the existing pressure solve). No new
heap allocation beyond one small static/persisted array (240 floats = 960 bytes, or could be
packed to 240 bytes/bits later if RAM ever became tight — not needed at this scale on a 64KB-SRAM
part). **The solver itself — `integrate_particles`, `push_particles_apart`, `transfer_velocities`,
`solve_incompressibility` — is not touched**; this lives entirely in the existing
solver-state-to-display-buffer conversion step (`update_cell_colors_from_types` in C,
`updateCellColors` in JS), which is already the designated single hand-off point per
`contracts/physics-core.md`. The buffer's size/contract (`GRID_X * GRID_Y`, 1:1 with the display)
is unchanged.

**Alternatives considered**:
- *Threshold the raw density with a single cutoff (no hysteresis)*: rejected — moves the noise
  problem rather than removing it; the density value passes through its most jitter-prone region
  (the boundary itself) right where a single threshold would sit, so it would still flip almost as
  often as the binary version.
- *Exponential moving average (low-pass filter) on the signal instead of hysteresis*: a valid,
  also-cited technique (sources below), but requires picking and tuning a decay rate and still
  needs *some* threshold afterward (so doesn't replace hysteresis, only could supplement it) —
  rejected as the *sole* fix for being strictly more tuning/compute than hysteresis alone for the
  same result; hysteresis on the existing density field already fully solves the reported problem.
- *Increase resolution/particle count to reduce surface noise at the source*: cited in the FLIP
  literature as a real fix, but directly contradicts the spec's STM32L4 compute budget (more
  particles = more pressure-solve and transfer work every tick) — rejected.
- *Minimum particles-per-cell filtering*: cited in PIC literature, but requires extra per-cell
  particle-count bookkeeping the solver doesn't currently expose at the display layer; reusing the
  already-computed density is simpler and cheaper.

**Empirical finding (important, measured after implementing)**: a side-by-side headless
comparison (settle 6000 steps under constant gravity, then count state changes over the next 600)
showed the binary signal flipping 193 times vs. the hysteresis signal flipping 134 times — a real
~30% reduction, confirming the fix removes the "any tiny move flips it" pathology. However,
**widening the hysteresis gap further (tested up to ON=0.9/OFF=0.05) barely moved the count
(125-134), and adding either an exponential-moving-average smoothing pass or a 20-frame minimum
dwell time on top had *zero* additional effect.** That null result is itself informative: it rules
out fast noise as the remaining cause (smoothing would have caught that) and rules out
rapidly-bouncing threshold crossings (a 20-frame dwell requirement would have caught that). The
remaining ~130 changes per 600 frames are **sustained** transitions — the densely-packed particle
block genuinely continues rearranging itself slowly over several real-world seconds, which is
authentic surface motion from this solver's parameters (`flipRatio = 0.9` is lightly damped by
design — Decision 11), not measurement noise. Suppressing it further would require either tuning
solver parameters (more separation iterations / more damping — explicitly out of scope here) or a
debounce delay long enough to also blunt genuine, deliberate gravity-direction changes — a worse
trade-off than the residual motion itself. The hysteresis fix is kept because it provably removes
the one-point-infinite-sensitivity defect; the remaining slow surface motion is left as accurate
physical behavior, not a remaining bug.

**Sources** (from the prior research conversation, retained here for traceability):
- [Particle-Based Simulation of Fluids (Utah CS)](https://www.sci.utah.edu/~tolga/pubs/ParticleFluidsHiRes.pdf) — particles-per-cell guidance, surface noise in coarse grids
- [Suppressing grid instability and noise in particle-in-cell simulation by smoothing](https://arxiv.org/html/2503.05123v1) — PIC particle noise and grid-smoothing passes
- [Flickering in FLIP simulation surface (od|forum)](https://forums.odforce.net/topic/63250-flickering-in-flip-simulation-surface/) — temporal averaging / weak-point filtering for surface flicker
- [Schmitt Trigger Circuit: Noise Immunity and Hysteresis Guide](https://zbotic.in/schmitt-trigger-circuit-noise-immunity-and-hysteresis-guide/) — dual-threshold mechanism
- [Proximity Sensor Hysteresis — Wintriss Controls](https://www.wintriss.com/wcg/knowledgebase/proximity-sensor-hysteresis.html) — same mechanism applied to sensor chatter

## Round 5 — Realistic display rendering within a fixed 16x15 grid (2026-06-21)

The user asked for the most robust, realistic-as-possible way to display the fluid on the fixed
16x15 matrix, explicitly noting the *particles* already behave like real water even though the
*display* doesn't, and asked that this be researched against other real fluid-pendant/simulator
projects on the internet, while staying STM32L4-appropriate with no complex calculations.

### What other real fluid-pendant projects actually do (researched, not assumed)

This project's whole premise (FLIP simulation + charlieplexed matrix + accelerometer pendant) has
a direct, well-documented ancestor: **mitxela's "Fluid Simulation Pendant"**
([mitxela.com](https://mitxela.com/projects/fluid-pendant), [Hackaday writeup](https://hackaday.com/2025/01/13/fluid-simulation-pendant-teaches-lessons-in-miniaturization/),
[Hackaday.io](https://hackaday.io/project/205649-fluid-simulation-pendant)) — it uses an
**STM32L432KC** (the same L4 family as our STM32L431CC), an **ADXL362** accelerometer (this
project's originally-assumed sensor before the MPU-6500 correction), a diagonally-charlieplexed
LED matrix, and Matthias Müller's FLIP tutorial (the exact algorithm family already in this
codebase) — confirming this project's overall architecture already matches established practice.
Critically for this question: that pendant's display is colored by **"density (number of
particles overlapping each grid cell)"** — i.e. the same `particleDensity` field this codebase
already computes — and the writeup explicitly says brightness control there is **system-wide**
("varying the voltage to the microchip... dims all pixels evenly"), **not per-LED PWM/greyscale**.
Realism there instead comes from a denser physical display (216 LEDs in a 25mm pendant, tiny 0402
pitch) and a watch-glass cover that optically diffuses/blurs adjacent LEDs together — both
hardware/optical factors, not firmware.

A second related project, a ["fluid simulation business card"](https://hackaday.com/2025/08/12/leds-that-flow-a-fluid-simulation-business-card/)
([source](https://github.com/Nicholas-L-Johnson/flip-card)), uses 441 LEDs (21×21) and "treats the
LEDs as particles in a virtual fluid" directly — again, realism via LED count high enough to
approach particle-level granularity, not via per-pixel brightness tricks. Neither reference
project's source disclosed any per-LED greyscale/PWM logic.

**Why this matters for our decision**: our display is fixed at 16×15 (240 LEDs) — a real PCB/pin
constraint, not something this software-planning round can change, so the "use way more LEDs"
lever both reference projects rely on isn't available to us. The lever that *is* available, and
that general charlieplexing literature independently confirms is a standard, well-established,
lightweight technique ([Charlieplexing — Arrow](https://www.arrow.com/en/research-and-events/articles/charlieplexing-an-led-matrix);
commercial drivers like the [IS31FL3741](https://www.adafruit.com/product/2946) do exactly this in
hardware), is **per-LED brightness via PWM/duty-cycle modulation within the charlieplex scan** —
something neither reference project needed (their displays were dense/diffused enough without it)
but that directly compensates for our coarser, fixed grid.

### Decision 13 — Continuous brightness from `particleDensity`, superseding Round 4's binary hysteresis

> **Superseded by Round 7's Decision 15** (below): the real STM32 charlieplex hardware turns out
> not to support per-LED PWM after all (confirmed against this project's own
> `sample-charlieplexing` reference driver), so the simulators were reverted to binary on/off —
> via the restored Decision 12 hysteresis, not a plain binary flag — to keep previewing what the
> real hardware can actually do. Left here for the reasoning trail (the research against real
> fluid-pendant projects is still valid context); see Decision 15 for what actually shipped.

**Decision**: stop rendering a binary on/off state at all. Render each display cell's brightness
directly as `clamp(particleDensity[cell] / particleRestDensity, 0, 1)` — a continuous 0.0–1.0
value, already exactly the data Round 4 normalized for its thresholds, just used directly instead
of being collapsed into a yes/no decision first. **This supersedes Decision 12's hysteresis
entirely** — `g_ledState`/`ledState` and the two thresholds are removed, not kept alongside it.

**Why this is more robust than Round 4's fix, not just different**: hysteresis was a patch for a
problem *created by* binarization (a continuous, physically-meaningful density value being forced
into a single bit before display). Rendering the continuous value directly removes the problem at
its source instead of managing symptoms of it — there is no longer a hard state to flicker
between, so a cell with a small residual density just glows faintly and steadily instead of
needing to decide whether it counts as "fluid." This directly reflects the user's own framing: the
*particles* (and now `particleDensity`, their direct grid projection) already behave realistically
— it was only the binary squashing step that was throwing that realism away.

**Why this is STM32L4-appropriate / no complex calculations**: the division and clamp are exactly
the same two operations Decision 12's normalization already did — this is strictly *less*
computation than before (no persisted state array, no branching dual-threshold comparison). No
new per-tick math, no spatial blur/smoothing pass across neighboring cells (rejected below), no
new physics. For the real hardware, this is a documented forward-looking note only (Stage 3
remains deferred per this project's scope) — the standard, cheap way to realize a continuous
brightness value on a charlieplexed display is **bit-angle modulation (BAM)**: precompute a
handful (e.g. 4–8) of binary on/off scan buffers per logical frame, one bit-plane each, and let the
existing TIM+DMA scan cycle through them — each LED's average brightness over one full frame is
then just "how many of the bit-plane buffers include it," set once per frame by a couple of bit
comparisons per cell, not a new per-LED PWM timer. This reuses the same DMA-driven, zero-CPU-
polling scan architecture research.md Decision 4 already established; no new driver concept.

**Alternatives considered**:
- *Keep Round 4's hysteresis, just widen/tune it further*: rejected — Round 4's own empirical
  testing already showed widening the gap and adding smoothing/dwell time had no effect, because
  the residual issue was information loss from binarization itself, not threshold placement.
  Continuous brightness fixes the actual cause.
- *Spatial blur/anti-aliasing across neighboring cells (e.g. averaging with adjacent cells before
  display)*: rejected — `particleDensity` is already a bilinear P2G-splatted value, i.e. already
  smoothed across its four nearest grid points; adding a second smoothing pass on top is genuinely
  new computation (a convolution-like step) for marginal extra benefit at this resolution, directly
  against the "no complex calculations" constraint.
- *Increase LED count / matrix resolution to match reference-project realism*: rejected — fixed
  hardware constraint, explicitly out of scope for this software round.
- *Per-pixel hue/color shift (e.g. depth-tinted blue/cyan) instead of brightness*: not investigated
  further — this project's LEDs are single-color per the existing rendering code (`rgb(0, g*255,
  0)` in the JS prototype, plain on/off `grid[i][j]` in the C app), so only intensity is available,
  not hue, without new hardware.

### Sources

- [mitxela.com — Fluid Simulation Pendant](https://mitxela.com/projects/fluid-pendant)
- [Hackaday — Fluid Simulation Pendant Teaches Lessons In Miniaturization](https://hackaday.com/2025/01/13/fluid-simulation-pendant-teaches-lessons-in-miniaturization/)
- [Hackaday.io — Fluid Simulation Pendant project log](https://hackaday.io/project/205649-fluid-simulation-pendant)
- [Hackaday — LEDs That Flow: A Fluid Simulation Business Card](https://hackaday.com/2025/08/12/leds-that-flow-a-fluid-simulation-business-card/)
- [Charlieplexing Tutorial: LED Matrix Panels — Arrow](https://www.arrow.com/en/research-and-events/articles/charlieplexing-an-led-matrix)
- [Adafruit 16x9 Charlieplexed PWM LED Matrix Driver (IS31FL3741)](https://www.adafruit.com/product/2946)

## Round 6 — Top display row (row 1) never lights up (2026-06-21)

The user reported that after Round 5's brightness rendering shipped (confirmed beautiful and
realistic via screenshot — graded surface, no hard binary edge), the topmost display row (labeled
"row 1" in the 1-based UI) never lights up at all, under any condition. They also separately
flagged the leftmost column showing ongoing up/down motion while the rest of the grid sits still
at rest — **investigated and explicitly accepted as genuine, realistic settling motion, not a
bug** (the user's words: "forget it, it looks beautiful and real this way"), the same category of
finding as Round 4's residual surface motion. No code change was made for that second item.

### Decision 14 — Fix the off-by-one that excludes the open top row from ever receiving density

**Root cause, confirmed empirically before any fix was written**: a 20,000-step headless sweep
(varying gravity direction violently, including transiently pointing it *upward*, which normal
operation never does) measured the maximum brightness ever reached in the top display row at
**0.000000** — proving this is a hard structural exclusion, not a rare/statistical "fluid rarely
gets up there" effect.

The cause is an inherited off-by-one in `update_particle_density()`'s (`flip_fluid.c`, and the
matching `updateParticleDensity()` in `flip.js`) bilinear density-splat neighbor clamp:

```c
int y1 = (y0 + 1 < f->fNumY - 1) ? (y0 + 1) : (f->fNumY - 2);
```

A particle's y-position is itself already clamped to a maximum of `(fNumY-1)*h` (in
`handle_collisions`/the position clamp), which makes `y0` naturally cap at `fNumY-2`. At that
value, the condition `y0+1 < fNumY-1` is false, so `y1` collapses to `fNumY-2` — equal to `y0` —
instead of advancing to `fNumY-1`. The result: **no particle, at any position, can ever contribute
density to grid index `fNumY-1`**, the single row this project's Decision 7 deliberately *un-walled*
specifically so it could hold visible fluid. This same pattern also exists in `transfer_velocities`'
neighbor clamp, used for both the particle-to-grid and grid-to-particle velocity transfer.

**Why this was invisible until now**: this is the original ten-minute-physics tutorial's own
boundary-clamp pattern (verified identical in this project's already-existing JS port before this
fix). In that original demo, *all four* sides are walled, so index `fNumY-1` is a solid wall cell
in the original design — never receiving density was always correct there, by coincidence, because
nothing should be there anyway. Decision 7 (Round 2) intentionally removed the top wall so that row
could legitimately hold fluid for display purposes, but this pre-existing clamp was never revisited
at the time — it kept behaving as if that row was still walled off, silently undoing half of
Decision 7's intent. This is a latent defect Decision 7 exposed, not something Round 5 introduced.

**Decision**: correct the upper-neighbor clamp so it can reach the true last valid index
(`fNumY-1`) instead of stopping one short of it, in **both** places this exact pattern appears
(`update_particle_density` and `transfer_velocities`, both files, both stages):

```c
int y1 = (y0 + 1 < f->fNumY) ? (y0 + 1) : (f->fNumY - 1);
```

The `x0`/`x1` clamp (same pattern, applied to the x-axis) is deliberately left untouched — both
the left and right walls are real, intentional solid cells (Decision 7's symmetric x-padding), so
excluding `fNumX-1` there is correct, matching the original design's intent exactly.

**Why this is not a "core simulation" change in the sense the user has repeatedly guarded against**:
no formula, parameter, or algorithm changes — `flipRatio`, `overRelaxation`, iteration counts,
gravity handling, and the solver's structure are all untouched. This corrects an array-index bound
to match what Decision 7 already established as the intended domain shape; it makes the existing
algorithm internally consistent with its own already-approved boundary change, rather than altering
how the algorithm behaves. `solve_incompressibility`'s own loop bound (`j < fNumY-1`, excluding the
top row from direct pressure correction) is **not** changed — that exclusion is the standard,
correct way to represent a free/open top surface (implicitly zero/atmospheric pressure there), and
remains appropriate now that the top is genuinely open.

**Alternatives considered**:
- *Leave the clamp and instead special-case the display to "borrow" a value for the top row from
  the row below it*: rejected — papers over the real defect with a second, unrelated approximation,
  and the velocity-transfer instance of the same bug would still silently misbehave near the top.
- *Rewrite the splat using a different neighbor-selection scheme entirely*: rejected — unnecessary;
  the existing scheme is correct everywhere except this one off-by-one, so the minimal fix is
  exactly that — fixing the one wrong constant, not replacing the technique.

### Sources

- Empirical verification: temporary `smoke_test.c` harness (deleted after use), 20,000-step sweep
  with deliberately violent/inverted gravity, confirmed 0.000000 max brightness in the top row both
  before and (separately) the expected non-zero result after the fix (see tasks.md Phase 10).
- User-supplied reference: [Matthias Müller's tenMinutePhysics FLIP tutorial](https://matthias-research.github.io/pages/tenMinutePhysics/18-flip.html),
  the algorithm this project's `flip_fluid.c`/`flip.js` already port — confirms the original design
  is walled on all four sides, explaining why this off-by-one was never visible there.

## Round 7 — Reverting to binary display: real charlieplex hardware has no PWM (2026-06-22)

The user realized, after Round 5 shipped, that the real STM32 charlieplex hardware **cannot**
actually do per-LED PWM/brightness — confirmed against this project's own existing reference
driver, `stm_projects/sample-charlieplexing/Core/Src/main.c`: its scan buffers
(`bsrr_buf[240]`/`moder_buf[240]`) hold exactly **one** on/off pattern per LED slot, driven once
per DMA-cycled scan pass. Decision 13's forward-compatibility note (cheap bit-angle-modulation PWM
"in the existing DMA-driven charlieplex scan") assumed that scan could cycle through *several*
such buffers per displayed frame — true in principle, but that would mean enlarging these fixed
240-entry buffers into multiple bit-plane buffers and restructuring the timing loop, which is
exactly the kind of added complexity this project has consistently been asked to avoid on the
STM32L4. The user's call: keep it simple, binary on/off, matching what the real driver can
actually do today.

### Decision 15 — Revert Decision 13; restore Decision 12's hysteresis (not plain binary `cellType`)

**Decision**: cells go back to a binary on/off `cellColor`, but **not** by reverting to the
original raw `g_cellType == FLUID_CELL` check Decision 12 first replaced. That binary signal is a
step function with all its sensitivity concentrated at one exact cell-boundary point — reinstating
it would bring back exactly the surface flicker Round 4 fixed. Instead, restore Decision 12's
hysteresis-on-density approach (two thresholds, `LED_ON_THRESHOLD`/`LED_OFF_THRESHOLD`, with a dead
zone between them, applied to the same already-computed `particleDensity`/`particleRestDensity`
Decision 13 used for brightness) — the output is binary again, but the decision of *when* to flip
is still based on the smooth, continuous density signal, so sub-cell particle jitter still can't
flip an LED every frame. This is the same code this project already wrote, tested, and verified
once before (Round 4) — being reinstated, not redesigned.

**Why not just revert to Decision 12's exact old code wholesale**: the underlying `cellColor`
buffers, function signatures, and surrounding code (e.g. `util.c`'s `grid[][]` brightness scaling,
Round 6's clamp fix) have moved on since Decision 12 was first written and then removed in Round 5.
The hysteresis *logic* is restored verbatim (same two thresholds, same dead-zone behavior, same
persisted per-cell state array); the surrounding plumbing (`grid[][]` in `util.c`, `cellColor` in
both files) is adapted to emit a clean 0/1 (not 0-255 brightness) again.

**Why this is still STM32L4-appropriate / no complex calculations**: identical cost to Decision 12
the first time it shipped — one persisted 240-cell array, two threshold comparisons per cell per
frame, no new physics, no per-LED PWM logic needed on the real driver (matching what it can
actually do).

**Alternatives considered**:
- *Keep continuous brightness in the Stage 1/2 simulators "for looks," accept it just won't match
  Stage 3's real binary hardware*: rejected by the user — the simulators exist specifically to
  preview real hardware behavior before flashing it (spec User Story 2's whole purpose), so showing
  a richer effect the real device can't reproduce would make the simulators actively misleading.
- *Revert straight to the original raw `cellType` binary check*: rejected — silently reintroduces
  the Round 4 flicker bug with no benefit, since the hysteresis fix costs nothing extra.

### Sources

- `stm_projects/sample-charlieplexing/Core/Src/main.c` (`bsrr_buf[240]`/`moder_buf[240]`) — this
  project's own existing charlieplex driver reference, confirming the single-buffer-per-scan
  structure that makes per-LED PWM nontrivial to add.

## Summary of resolved unknowns

| Unknown | Resolution |
|---|---|
| Target MCU (constitution said STM32F103C8T6; user said "STM32 L4") | STM32L431CC, confirmed by user-provided ST datasheet link + `axelor.ioc` |
| Numeric type for physics core | `float` only, no `double`, no fixed-point |
| Pressure solver iteration count for real-time budget | Reduce from 100 to ~20, tune on hardware; same SOR/Gauss-Seidel algorithm |
| Simulation grid resolution vs. display resolution | 1:1 — resize sim grid to 16x15 to match the LED matrix exactly |
| LED matrix drive mechanism | Reuse TIM+DMA GPIO `BSRR`/`MODER` pattern from `sample-charlieplexing`, scaled to 16 pins |
| Sensor part and read mechanism | **MPU-6500** (corrected from ADXL362), accelerometer axes only, gyro disabled; interrupt-driven SPI1 read in Low-Power Accelerometer Mode using the part's Data Ready interrupt |
| Border LEDs (left/right/bottom) never lit | Solver grid padded 1 cell beyond the display on walled sides (`fNumX=GRID_X+2`, `fNumY=GRID_Y+1`); display `cellColor` contract stays `GRID_X*GRID_Y` |
| Stage 1 felt much "lighter" than Stage 2 | Single found difference was `numPressureIters` (100 vs. 20) — matched Stage 1 to Stage 2's value |
| Missing cell separators/row-col labels, oversized Stage 2 window | Rendering-only fixes (`CELL_SIZE`, border-drawing, static text labels) — zero change to simulation-space scale |
| Stage 1 start/pause was keyboard-only | Added visible Start/Pause buttons matching Stage 2's UI pattern |
| Stage 2 played back in slow motion (~42% real speed) vs. Stage 1's near-real-time | `scene.dt` changed to match Stage 2's actual 20ms tick interval, at the same 50Hz tick rate (no added compute) |
| Surface LEDs flicker at rest | Hysteresis threshold on the already-computed `particleDensity`/`particleRestDensity` (Decision 12) — reinstated by Decision 15 after a brief detour through Decision 13 |
| Display looks unrealistic / too binary for a fixed 16x15 grid | Render continuous brightness directly from `particleDensity`/`particleRestDensity` (clamped 0-1) — **superseded by Decision 15**: real charlieplex hardware has no per-LED PWM, so this was reverted |
| Top display row (row 1) never lights up | Off-by-one in the density-splat/velocity-transfer upper-neighbor clamp excluded grid index `fNumY-1` entirely (harmless in the original all-walls-closed tutorial design, exposed once Decision 7 opened the top wall); fixed by correcting the clamp to reach the true last index (Decision 14) |
| Real charlieplex hardware has no per-LED PWM | Reverted Decision 13's continuous brightness; restored Decision 12's hysteresis-on-density binary output (Decision 15) — same flicker-free behavior, matches real hardware capability |
