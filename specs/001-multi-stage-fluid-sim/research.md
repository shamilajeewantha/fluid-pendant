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

## Round 8 — axelor Stage 3 bring-up begins: accel telemetry + placeholder display pattern (2026-06-22)

Stage 3 (firmware) was explicitly deferred by this plan's original Structure Decision — "the user
will initiate the STM32CubeIDE project themselves as a separate, later phase." That phase has now
started, directly inside `stm_projects/axelor` (the same board this plan's Hardware Constraints
section already names). Across the preceding sessions the user independently brought up the
charlieplex DMA scan (TIM1 + DMA-to-`GPIOB->BSRR`/`MODER`, modeled on `sample-charlieplexing` per
Decision 4) and the MPU-6500 SPI read path (`MPU6500_ReadAccel()`, raw register → milli-g, per the
sensor part correction in Decisions 5/contracts/sensor-driver.md) to the point that both build,
flash, and run correctly on real hardware — confirmed by the user ("ok great now all leds light up
+ serial uart works too"). This round scopes the *next* two concrete, narrow pieces of work,
explicitly **excluding** wiring either one into the actual physics core yet:

1. Extend the existing UART telemetry to also print the accelerometer reading in the exact units
   `MotionInput` will eventually need (`m/s²`, not raw LSBs or `mg`), so the conversion is verified
   on real hardware *before* anything calls `simulateFlipFluid` with it.
2. Replace the current "light every one of the 240 slots, unconditionally, forever" bring-up test
   pattern with a periodically-refreshing pattern over a **new buffer that sits logically where
   `DisplayFrame`/`cellColor` will sit once the physics core is ported** — explicitly not the DMA
   scan mechanism itself, which stays exactly as validated.

All edits for this round are confined to `stm_projects/axelor` (per explicit user instruction); no
other stage's files change.

### Decision 16 — Accel telemetry: print raw register values *and* the `m/s²` value `MotionInput` needs, side by side

**Decision**: extend `MPU6500_ReadAccel()`'s existing UART print to add a second line showing each
axis converted all the way to `m/s²` (`accel_mps2 = (accel_mg / 1000.0f) * 9.81f`), printed
alongside the already-existing raw register values and milli-g values. No new read path, no new
sensor mode, no interrupt-driven rework yet — same polled SPI read this module already does at the
same point in the existing 500ms loop tick.

**Rationale**: data-model.md's `MotionInput` entity is defined as `x`/`y` in plain `float`, unit-
compatible with `Scene.gravity_x`/`gravity_y` (themselves `m/s²`, magnitude up to 9.81 at rest per
the existing desktop slider's `sin(angle)*9.81`/`-cos(angle)*9.81` in `util.c`/`script.js`). The
firmware's MPU-6500 driver currently stops one conversion short of that, at milli-g
(`accelX_mg`/etc. — already added). Printing the final `m/s²` value over UART now, well before any
simulation code exists on this board, lets the user visually confirm on real hardware that tilting
the board produces the *correct-sign, correct-magnitude* numbers `MotionInput` expects — catching
an axis or sign mistake here, against a printed number, is far cheaper than catching it once it's
silently driving (or mis-driving) a running fluid simulation. This directly addresses the open gap
flagged in this same session: which of the MPU-6500's three physical axes are in the display's
plane, and with what sign, is **unknown without testing on the real board** — printing both
raw-and-formatted values side by side, then physically tilting the board four ways while reading
the UART output, is the cheapest way to resolve that gap empirically rather than by assumption.

**Explicitly not in scope for this decision** (per the user's "NO SIM INTEGRATION YET"): no value
from this print path is read by, or written into, any `Scene`/`FlipFluid` field — there isn't one
on this board yet. No low-pass filtering or dead-banding is added yet either (contracts/
sensor-driver.md already specifies that as a requirement for the *eventual* full driver, once
sim integration happens) — this round's job is exclusively to make the converted number visible
and verifiably correct, not to start consuming it.

**Alternatives considered**:
- *Print only the final `m/s²` value, drop the existing raw/mg lines*: rejected — the raw/mg lines
  are what let the user notice if the *conversion itself* is wrong (e.g. comparing a raw register
  swing against the expected `m/s²` swing for a known tilt angle); removing them would make a
  conversion bug invisible.
- *Add the unit conversion inside a new, separate function*: rejected as unnecessary structure for
  one multiply; it's added inline in the existing print, same as the existing raw→mg conversion
  already is.

### Decision 17 — A periodically-refreshing pattern buffer, decoupled from the DMA scan table

**Decision**: introduce a new 240-entry buffer (one entry per charlieplex slot, same indexing as
the existing `bsrr_buf`/`moder_buf`) that is **not** read by DMA and **not** the thing TIM1 scans —
call it the *frame buffer* (the firmware-local stand-in for `DisplayFrame`/`cellColor` in
data-model.md, until the physics core is actually ported onto this board). Once per existing
500ms main-loop tick (reusing the loop's existing cadence — no new timer needed), a pattern
generator fills the frame buffer with a new pattern, and a small translation step rewrites only
`moder_buf` (not `bsrr_buf`, which keeps the same fixed pin-pair-per-slot encoding it already has)
so that only the slots marked "on" in the frame buffer are actually driven as outputs by the scan;
everything else reverts to plain input (Hi-Z) for that slot, same as a never-populated slot already
does today. The existing `ChargePlex_ValidateScanTable()` self-check (which currently requires
*every* slot to have exactly 2 output pins) MUST be relaxed to also accept a slot with **zero**
output pins as valid (an "off" slot) — the existing invariant for slots that *are* on (exactly one
source-high pin, one different sink-low pin, all others untouched) is unchanged and still runs on
every regenerated frame before it is allowed to go live, for the same reason it was added in the
first place: this board has no per-leg current-limiting resistors, so a malformed frame must fail
safe (skip the update, keep the previous known-good frame) rather than ever reach the DMA buffers.

**Pattern chosen**: a "sweeping window" — a contiguous run of `K` consecutive slot indices (in the
existing raw `(src,dst)` enumeration order from `ChargePlex_BuildScanTable`) marked "on," with the
window's starting offset advancing by a fixed step each 500ms tick and wrapping modulo 240. This
needs only one persisted integer (the current offset) and integer modulo arithmetic — no floats,
no trig, nothing the existing pressure-solver-budget conversation about STM32L4 compute headroom
needs to weigh in on. Choosing `K` (window width) and the step size are tuning knobs left for
implementation/on-hardware tuning (tasks.md), not fixed here.

**Why a pattern defined in raw slot-index order, not real (row, col) display positions**: this
project still does not have a confirmed mapping from a charlieplex `(src,dst)` pin pair to its
physical position in the 16×15 grid — that depends on the matrix's physical wiring, which hasn't
been provided. Defining "beautiful" in terms of slot order rather than guessed grid geometry keeps
this round honest: it demonstrates and exercises the new buffer-and-translation mechanism (which
*is* the reusable part — `DisplayFrame`/`cellColor` will plug into the exact same frame buffer
later) without asserting a geometric claim ("this is a wave moving left-to-right") that can't yet
be verified against the real board's actual wiring.

**Refresh cadence**: ~0.5s per frame update, **explicitly decoupled from the DMA scan rate** — the
existing TIM1+DMA scan continues cycling through all (currently-enabled) slots at its own much
faster, unchanged rate (~28.8 kHz/slot, full-240-slot pass in ~8.3ms, per the existing `Period`/
`Pulse` values already verified on hardware). The 0.5s figure governs only how often the *frame
buffer's content* changes, independent of and far slower than how fast the DMA hardware re-reads
whatever is currently in it. The user explicitly flagged this as a deliberately low starting point,
reusable later to probe DMA/refresh performance margins — not a hard requirement.

**Alternatives considered**:
- *Random pattern each tick*: rejected — not reproducible/debuggable, and "covers all of the LEDs"
  is not guaranteed within any bounded time the way a deterministic sweep guarantees it.
- *Static checkerboard on slot-index parity (even/odd slots on)*: rejected — satisfies "covers all
  the LEDs" only once (never changes), which doesn't address "I really cannot see much" (a static
  half-lit pattern is barely more informative than the current all-on pattern) — the user asked for
  something that visibly refreshes.
- *Rewrite `bsrr_buf` as well as `moder_buf` per frame*: unnecessary — each slot's source/sink pin
  *identity* never changes, only whether that slot is driven at all; only `moder_buf` needs to
  change to turn a slot on/off, so `bsrr_buf` is written once at boot (as today) and never again.
- *Drive the frame-buffer refresh from a hardware timer interrupt instead of the existing main
  loop*: rejected for this round — the existing 500ms `HAL_Delay`-based loop already ticks at
  almost exactly the requested cadence; adding a second timer/ISR for the same 0.5s period would be
  more new mechanism than this scoped, placeholder-pattern round needs.

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
| `MotionInput` units the accel driver must ultimately produce | `m/s²` (matches `Scene.gravity_x/y`); firmware now prints this alongside raw/mg values for on-hardware verification before any sim code consumes it (Decision 16) |
| How to show a refreshing pattern on real hardware before the physics core exists | New frame buffer (240 slots, same indexing as `bsrr_buf`/`moder_buf`) filled by a deterministic "sweeping window" pattern generator every ~0.5s, translated into `moder_buf` only (`bsrr_buf` unchanged); decoupled from the DMA scan's own much faster rate; this is the same buffer `DisplayFrame`/`cellColor` will populate once ported (Decision 17) |
| Real charlieplex hardware has no per-LED PWM | Reverted Decision 13's continuous brightness; restored Decision 12's hysteresis-on-density binary output (Decision 15) — same flicker-free behavior, matches real hardware capability |

## Round 9 (2026-06-23) — porting the physics core onto `axelor`, real sim now drives the LEDs

User copied `final_project/flip_sim_c`'s contents into `stm_projects/axelor/Core/Src/flip_sim_c/` and
asked for the placeholder pattern generator (Decision 17) to be replaced by the real ported
simulation, driven by the already-wired accelerometer, while preserving every safety mechanism
built in Rounds 7–8. Edits remain confined to `stm_projects/axelor` per the user's standing
instruction; `final_project/flip_sim_c` itself is untouched.

### Decision 18 — Which copied files actually get ported, which get discarded

**Decision**: only `flip_fluid.c`/`.h`, `flip_utils.c`/`.h`, and `scene.c`/`.h` are adapted into the
firmware build. `main.c`, `util.c`, `util.h`, the `Dockerfile`/`.dockerignore`, `Makefile`,
`README.md`, and the prebuilt `main.exe` are **not** part of the firmware — confirmed by inspection
(`util.h`/`util.c`/`main.c` `#include <windows.h>`/`<commctrl.h>` directly; they are Stage 2's
Win32 GUI/trackbar glue, per `contracts/physics-core.md`'s own scoping of which files are the
actual physics-core contract).

**Rationale**: `contracts/physics-core.md` already states the physics core "MUST compile and run
with no STM32 HAL includes... and no `<windows.h>` dependency" — checking the actual copied files
confirms `flip_fluid.{c,h}`, `flip_utils.{c,h}`, `scene.{c,h}` satisfy this today (verified via
grep: their only `#include`s are `<math.h>`, `<stdlib.h>`, `<string.h>`, `<stdio.h>`,
`<stddef.h>`, and each other's headers) — they were already portable without modification to their
includes. `util.*`/`main.c` are the one part of the copy that is categorically Stage-2-only and
must not be dragged into the firmware build.

**Alternatives considered**: porting `util.c`'s rendering logic into a debug-only UART dump —
rejected as unnecessary scope; the existing `[CHARLIEPLEX] frame: row N lit` print plus the matrix
itself is sufficient on-hardware visibility, and `util.c`'s trackbar/window code has no embedded
equivalent worth building this round.

### Decision 19 — `malloc`/`calloc` replaced with static fixed-size arrays

**Decision**: every dynamic allocation in `flip_fluid.c` (`g_u`, `g_v`, `g_du`, `g_dv`, `g_prevU`,
`g_prevV`, `g_p`, `g_particleDensity`, `g_cellType`, `g_numCellParticles`,
`g_firstCellParticle`, `g_cellParticleIds`, `g_particleVel`) and `flip_utils.c` (`FlipFluid`
itself, `particlePos`, `s`, `cellColor`) becomes a `static` array sized to this project's fixed,
compile-time-constant tank/grid parameters, with `ensure_grid_alloc`/`ensure_alloc_particle_vel`'s
re-allocate-if-bigger logic deleted (size never changes after the one fixed `setupScene()` call).

**Rationale**: this project has exactly one tank configuration (`GRID_X=15`, `GRID_Y=16`,
`PAD_FNUM_X=17`, `PAD_FNUM_Y=17`, `relWaterWidth/Height=0.8`) — every dimension `setupScene()`
currently computes from constants is itself a constant; nothing about this firmware ever creates a
second `FlipFluid` or resizes one at runtime. Hand-tracing `setupScene()`'s formula gives
`numX≈17`, `numY≈19`, `maxParticles≈323`; sizing every array statically to that fixed maximum costs
roughly 21 KB of `.bss` (computed: ~2.6 KB × 2 for `particlePos`/`particleVel`, ~1.2 KB × 2 for `s`/
`cellColor`-class buffers, ~8 KB across the seven `wantCells`-sized velocity/pressure grids, ~3.6 KB
across the per-cell particle-bucket arrays) — comfortably under the constitution's 80%-of-64KB-SRAM
budget (51.2 KB) even before accounting for `axelor`'s existing charlieplex buffers (`bsrr_buf`/
`moder_buf`/`moder_template` at 240×4 bytes each ≈ 2.9 KB combined, plus `cpFrameBuf`/
`cpCandidateModer`). Eliminating the heap entirely on a bare-metal target with no OS removes a
whole category of risk (allocation failure with no handler, fragmentation, double-free) that this
project has no use for, since nothing it does actually needs a *variable*-sized allocation —
exactly the kind of unnecessary-complexity-removal this project has favored throughout (constitution
Principle I: minimize global mutable state; here, fixed-size statics are more predictable than the
heap, not less).

**Alternatives considered**:
- *Keep `malloc`/`free`, rely on newlib-nano's `_sbrk`-backed heap*: rejected — works, but adds a
  whole subsystem (heap implementation, `_sbrk` stub, a heap size decision in the linker script)
  for zero benefit given every size is already a fixed constant; also reintroduces the
  allocation-can-fail class of bug this round has no need to accept.
- *Keep dynamic sizing in case tank parameters become user-configurable later*: rejected as
  speculative — no current requirement asks for runtime-configurable tank size, and Principle I
  disfavors designing for hypothetical future requirements.

### Decision 20 — `floor` → `floorf` (constitution Principle IV compliance) — CORRECTED, no-op

**Original decision (now retracted)**: this entry originally claimed `flip_utils.c:39` had a lone
non-`f`-suffixed `floor` call needing a `floorf` fix.

**Correction (implementation phase, Round 9)**: that finding was a false positive from an
imprecise grep pattern (`floor[f]?`) that matched the *prose word* "floor" inside a code comment
("...floor-division round-trip risks off-by-one errors...") at that line, not an actual function
call. Re-verified directly against the as-ported `Core/Src/flip_fluid.c` and `Core/Src/flip_utils.c`
with `grep -n "[^f]floor("` (a pattern that only matches `floor(` not preceded by `f`) — **zero
matches** in either file. Every real `floor`-family call site in this codebase (in
`ensure_particle_hash_alloc`, `push_particles_apart`, `update_particle_density`,
`transfer_velocities`) already correctly uses `floorf`, confirmed by inspection during the T089
static-array conversion.

**Conclusion**: there is no `floor`→`floorf` bug to fix. tasks.md's T091 is corrected to record
this as a verification-only task (no code change made), rather than performing a no-op edit.
Constitution Principle IV's `f`-suffixed-math requirement was already satisfied by this source
before Round 9 began.

### Decision 21 — Simulation tick cadence decoupled from the existing 500ms print/blink loop

**Decision**: the main loop's per-iteration period is shortened from `HAL_Delay(500)` to
`HAL_Delay(20)` — matching `scene_default()`'s `dt=0.02f` (20ms/50Hz) exactly, so simulated time
advances at the same rate as real time (the same real-time-matching principle already established
in research.md Decision 11 for Stage 2). The verbose UART prints (`Counter:`, `[ACCEL] raw=.../
mm/s2=...`, the old per-frame `[CHARLIEPLEX] frame: row N lit`) are throttled to fire once every 25
iterations (~500ms) via a counter-modulo check, rather than every 20ms tick — printing at full
20ms-tick rate would mean each iteration's multiple blocking `HAL_UART_Transmit(..., HAL_MAX_DELAY)`
calls (each one several milliseconds at 115200 baud for these message lengths) eat most or all of
the 20ms budget themselves, starving the simulation step of the time it's actually supposed to run
in.

**Rationale**: constitution Principle IV requires each simulation tick to complete within 50ms and
the display to sustain ≥10 FPS; `dt=0.02f` was specifically chosen in Stage 2 (Decision 11) to match
a 50Hz real-world tick, and there is no reason for the firmware to use a different relationship
between simulated and real time than the already-validated Stage 2 desktop build uses. Decoupling
the print cadence from the simulation cadence (rather than slowing the simulation down to match the
old print-friendly 500ms loop) keeps the *physics* on the schedule that was actually tuned and
validated in Stage 2, while still giving readable, non-overwhelming UART output.

**Alternatives considered**:
- *Keep the 500ms loop period, run multiple internal `simulateFlipFluid()` substeps per iteration
  (fixed-timestep accumulator) to "catch up"*: viable in principle, but adds an accumulator/substep-
  count concept purely to preserve a print cadence that has no inherent meaning — rejected as
  unnecessary complexity when simply shortening the loop period and throttling prints achieves the
  same real-time accuracy more directly.
- *Move the simulation into a hardware-timer ISR, separate from the main loop*: rejected for this
  round — would touch the same kind of new-interrupt-mechanism territory the project has
  deliberately avoided adding for the display refresh in Decision 17, and the main loop's existing
  `chargePlexFaulted` gating/fail-safe structure already lives in `while(1)`; moving the tick
  elsewhere would require re-deriving those interactions from scratch for no requirement that asks
  for it.

### Decision 22 — Accelerometer reading feeds `simulateFlipFluid` as `gravity_x`/`gravity_y`, with a magnitude clamp

**Decision**: each tick, the already-computed `accelX_mps2`/`accelY_mps2` (Decision 16, `MPU6500_
ReadAccel()`) are clamped to a maximum magnitude of **`MAX_GRAVITY_MPS2 = 19.62f`** (the MPU-6500's
own ±2g full-scale range, `ACCEL_CONFIG.AFS_SEL=0` per `MPU6500_Init()` — confirmed via
spec.md Clarifications, Session 2026-06-23) before being passed as `simulateFlipFluid`'s
`gravity_x`/`gravity_y` parameters — directly, with no intermediate `MotionInput` struct introduced
this round (the existing two `float` locals already are that shape; adding a struct around two
already-adjacent floats is not justified by anything this round needs).

**Rationale**: spec edge case "What happens when the device is shaken hard or experiences a sudden
acceleration spike? The displayed fluid MUST remain visually stable" and data-model.md's
`MotionInput` validation rule "Magnitude clamped to a maximum (prevents a violent shake from
injecting an unbounded force into `integrate_particles`)" both require this; without it, a hard
shake could feed a multi-`g` spike directly into the solver's particle integration step, which
(per `flip_fluid.c`'s `integrate_particles`) has no internal clamping of its own — an unclamped
spike risks particles being flung outside the padded solver grid in one step, which is exactly the
kind of instability the existing FLIP solver was not written to recover from gracefully. Clamping at
the sensor/glue boundary (rather than inside the physics core, which the constitution and
`contracts/physics-core.md` both require stays a verbatim, unmodified port) keeps the clamp in the
one place (`main.c`, glue code) that is allowed to change.

**Alternatives considered**:
- *Clamp inside `integrate_particles`*: rejected — would modify the ported physics core itself,
  violating `contracts/physics-core.md`'s "ported verbatim" contract and constitution Principle III's
  module-boundary rule; the clamp belongs in the sensor-to-physics glue, not the physics.
- *Low-pass filter instead of a hard clamp*: deferred — `contracts/sensor-driver.md`'s Round 8 status
  already flags dead-banding/low-pass filtering as not-yet-implemented, open work for whenever the
  full interrupt-driven sensor contract is built; a hard magnitude clamp is the minimum needed to
  satisfy the spec edge case today without expanding this round's scope into the sensor driver
  rewrite.

### Decision 23 — `cellColor` wired directly into `cpFrameBuf`, replacing `ChargePlex_GeneratePattern()`'s placeholder sweep; nothing below `ChargePlex_ApplyFrameBuffer()` changes

**Decision**: `update_cell_colors_from_types()` (in the ported `flip_fluid.c`) already writes
`outColors[dstY * GRID_X + dstX] = 0.0f or 1.0f` — confirmed by reading it directly: `dst = dstY *
GRID_X + dstX`, i.e. **row-major with row=`dstY` (0..15), col=`dstX` (0..14)** — which is *exactly*
`cpFrameBuf[row][col]`'s existing shape (`CP_ROWS=16`, `CP_COLS=15`, confirmed identical to `GRID_Y`/
`GRID_X`) with no transpose or remapping needed. `ChargePlex_GeneratePattern()` is deleted entirely
and replaced by a tick that calls `simulateFlipFluid()` then copies `f->cellColor[row*GRID_X+col] >
0.5f` into `cpFrameBuf[row][col]`. `ChargePlex_ApplyFrameBuffer()`, `ChargePlex_ValidateScanTable()`,
`ChargePlex_BuildScanTable()`, `bsrr_buf`, `moder_template`, `ChargePlex_ForceSafeState()`,
`ChargePlex_CheckAlive()`, and the TIM1/DMA setup in `main()` are **not modified by this round** —
the simulation only ever gets to influence `cpFrameBuf`, the exact same upstream-of-the-validator
boundary the placeholder pattern generator wrote through in Round 8. This is the direct answer to
"is this a safety loophole": the sim is exactly as constrained as the placeholder sweep was — it can
only express plain per-cell on/off intent; it has no path to write `moder_buf`, `bsrr_buf`, or any
register directly, and every frame it produces still goes through the unchanged validate-or-reject
gate before any of it reaches hardware.

**Rationale**: this is precisely the hand-off `contracts/physics-core.md` already specifies
("Outputs: the `FlipFluid.cellColor` buffer... this is the sole hand-off point to the display
layer") and `data-model.md`'s `AxelorFrameBuffer` entity already anticipated ("This is the exact
slot the eventual `DisplayFrame`/`cellColor` output is expected to fill once the physics core is
ported — the pattern generator is a placeholder producer for the same consumer... not a separate
mechanism that integration would need to replace") — Round 8 deliberately built this boundary so
that swapping the placeholder for the real thing would be exactly this small.

**Alternatives considered**:
- *Have the simulation glue write `moder_buf` directly, skipping the frame-buffer/validator step
  "since the sim is already trustworthy"*: rejected outright — this is the one place this round
  must not cut a corner; the entire reason `ChargePlex_ApplyFrameBuffer`'s validate-or-reject gate
  exists is to fail safe against *any* upstream bug (placeholder generator or real physics, it makes
  no difference), and a porting/indexing bug in freshly-adapted physics code is at least as likely
  as a bug in the placeholder sweep that needed fixing twice already this project.

### Decision 24 — `Scene.paused` forced false; no pause/run UI exists on this board

**Decision**: `scene_default()`'s `paused = true` (a Win32-GUI-appropriate default — Stage 2 starts
paused until the user clicks Start) is overridden to `false` immediately after `scene_init()` in
`axelor`'s glue code, since there is no button or control on the pendant to ever un-pause it
otherwise.

**Rationale**: spec acceptance scenario requires the device to "keep producing a valid, stable
displayed pattern continuously during normal operation" — a permanently-paused simulation would
satisfy "stable" vacuously but not "valid... reacting to motion," defeating the entire feature.
`scene.c` itself is not modified (its default stays `true`, correct for Stage 2's own UI-driven
flow per `contracts/physics-core.md`'s "ported verbatim" rule); only the one field assignment in
`axelor`'s own glue code differs from the default.

## Round 10 — Physical row/column mapping investigation (2026-06-23, on-hardware findings, three passes — firmware fix ultimately retracted)

**Decision (final, Pass 3)**: no firmware change. `axelor`'s `cellColor`→`cpFrameBuf` copy step
writes to `cpFrameBuf[row][col]` directly, exactly as Decision 23 originally specified, with no
row/column transform of any kind. The root cause turned out to be a physical wiring defect on the
user's board, now corrected by the user in hardware — not a firmware/software mapping bug at all.
Passes 1 and 2 below produced a firmware compensation for that wiring defect; once the wiring
itself was fixed, that compensation became not just unnecessary but actively wrong (it would
mis-map an already-correct raw signal), so it was removed in Pass 3.

**Pass 1 (offset only)**: the user ran `stm_projects/axelor-test` (a backup copy that still has
Round 8's per-row sweep pattern and its `[CHARLIEPLEX] frame: row N lit` UART print) and reported,
on real hardware: firmware row index 0 (the first row lit at boot) physically lights up at what
they counted as the board's 8th row, and increasing the firmware row index moves the lit row
consistently in one direction from there. This is a board-wiring fact
(`ChargePlex_BuildScanTable`'s `src` pin-enumeration order doesn't happen to match the PCB's
physical row order), not a defect in the validator, DMA scanner, or the simulation itself. The
first fix applied was `physicalRow = (row + 8) % CP_ROWS` — reasoned (incorrectly, see Pass 2) to
be direction-independent since 8 is an exact half of `CP_ROWS = 16`, so a forward or backward
rotation by that exact distance lands on the same slot either way. That reasoning is true for the
*anchor point* but does not make the overall *sweep order* direction-independent — see Pass 2.

**Pass 2 (direction correction + column mapping)**: per the user's explicit request, `axelor-test`
was extended with a dedicated calibration sweep (replacing its `ChargePlex_GeneratePattern`):
raw, untransformed row index 0→15 lit one at a time (top of the function's loop), then raw column
index 0→14 lit one at a time, looping forever, each step printed over UART
(`[CHARLIEPLEX] ROW n of 16 ...` / `COLUMN n of 15 ...`) — deliberately built in `axelor-test`, not
`axelor`, per explicit instruction to keep `axelor` frozen until this was understood. On real
hardware, the user reported: rows swept "down to up" and columns swept "right to left" — both the
*opposite* of the intended top-to-bottom / left-to-right reading order. This is new information
Pass 1 didn't have: the raw firmware index's *direction* of travel (not just its starting point)
needed correcting, and the column dimension needed a mapping for the first time (previously
flagged as an open, unconfirmed gap in `contracts/display-driver.md`).

The corrected row formula keeps Pass 1's confirmed anchor (raw index 0 ↔ the physical row the user
counted as "8") but negates the step direction: `(8 - row + 16) % 16` instead of `(row + 8) % 16`
— at `row = 0` both formulas still give `8` (anchor preserved); for every other `row` they diverge
(direction corrected). The column formula is a pure mirror (`14 - col`), since no offset/anchor
asymmetry was reported for columns, only a direction reversal.

Both Pass 1 and Pass 2 fixes were applied at the exact same point Decision 23 already established
as the only place the Round 9 producer is allowed to touch (upstream of
`ChargePlex_ApplyFrameBuffer`'s validate-or-reject gate) — `ChargePlex_BuildScanTable`,
`ChargePlex_ValidateScanTable`, `ChargePlex_ApplyFrameBuffer`, `ChargePlex_ForceSafeState`,
`ChargePlex_CheckAlive`, `bsrr_buf`, and `moder_template` were never touched by either pass.
Build-verified (headless STM32CubeIDE build) after every pass, including the Pass 3 revert: 0
errors, 0 warnings.

**Pass 3 (retraction — root cause was a wiring defect, not a mapping bug)**: after Pass 2 was
applied to `axelor`, the user re-ran `axelor-test`'s raw row+column sweep again and reported it
now sweeps exactly as expected — rows top-to-bottom, columns left-to-right, with no offset and no
direction reversal — because they had found and fixed a physical wiring mistake on the board in
the meantime. This means the raw, untransformed `(src,dst)` enumeration from
`ChargePlex_BuildScanTable` now correctly corresponds to the board's physical row/column layout on
its own, with the hardware corrected. Pass 1's and Pass 2's firmware-side compensation (the row
offset+reversal and column mirror) was therefore retracted from `axelor`'s `main.c` — applying it
on top of now-correct wiring would have reintroduced exactly the scrambling it was built to fix.
**Lesson for future rounds**: when on-hardware row/column behavior looks wrong, check for a wiring
mistake before reaching for a firmware-side coordinate transform — this round spent two passes
building and refining a compensation for a problem that the right next step (re-checking the
physical wiring) resolved directly.

**Pass 4 (accelerometer axes swapped relative to display)**: running the real simulation on the
now-rewired board, the user found that holding the pendant upright in its normal worn orientation
made the fluid settle toward the display's *right edge* (a column position) instead of the bottom
row, even though the row/column wiring itself (Pass 3) was confirmed correct. Rotating the pendant
90° made it settle at the bottom correctly, which pins this down to a pure axis swap rather than a
sign/mirroring issue: the MPU-6500's physically-mounted X axis is the board's *vertical* axis and
its Y axis is the board's *horizontal* axis — backwards from what `main.c` originally assumed.
Fixed in `main.c`'s main loop by swapping which accel reading feeds which `simulateFlipFluid`
parameter: `clampedGravityX` now reads `accelY_mps2` and `clampedGravityY` now reads
`accelX_mps2` (matching `flip_fluid.c`'s `dstY*GRID_X+dstX` convention, where `gravity_y` drives row
motion and `gravity_x` drives column motion). `MPU6500_ReadAccel()` itself and `simulateFlipFluid`'s
parameter semantics are unchanged — this is board-specific glue code only. Build-verified: 0
errors, 0 warnings, both configs.

This resolves the "still open" display-orientation question below for the *horizontal* axis. The
*vertical* half of that question (whether `cellColor` row 0, the simulated floor, lands at the
pendant's physical bottom edge once gravity correctly pulls toward the right axis) remains pending
T099/T100's on-hardware run, now with both the row/column wiring (Pass 3) and the gravity-axis
mapping (Pass 4) corrected.

**Still open**: whether `cellColor` row 0 (the simulated tank's floor, where fluid pools when
`gravity_y` pulls it toward the wall at solver `y=0`) visually lands at the pendant's physical
bottom edge — i.e., whether the *display orientation* (not just the raw row/column order, which is
now confirmed correct) matches intuitive "fluid falls down" behavior — is exactly what
`quickstart.md` Round 9 checks 2–4 (tilt response, settling, shake stability) are designed to
catch, and remains open pending T099/T100's on-hardware run against the now-corrected `main.c`.

## Round 11 — Tilt-to-display latency (2026-06-23)

**Symptom**: tilt-to-visible-LED latency ~3s, vs. spec SC-004's <500ms. Persisted (only "slightly"
better) after switching Debug (-O0) -> Release (-Os).

**Checked, ruled out**:
- FPU: already hardware, not software-emulated. `.cproject` has `fpv4-sp-d16` +
  `floatabi=hard` on both Debug and Release. No change needed.
- `ChargePlex_ApplyFrameBuffer`/`ValidateScanTable`: O(240 slots x 16 pins) bitwise checks per
  tick — microseconds, not the bottleneck. Untouched either way (safety-critical, frozen).
- UART prints: already gated (every 25th tick), not unconditional.
- `MPU6500_ReadAccel`: one 7-byte blocking SPI transfer per tick, no added delay.

**Root cause (architectural, not a single hotspot)**: `main.c`'s loop computed `dt` as a fixed
0.02f (assumes exactly 20ms/tick) but called `HAL_Delay(20)` *after* all per-tick compute — so
real tick time = compute time + 20ms, while the physics was only ever told 20ms passed. Any
per-tick compute cost (whatever its size) silently became slow-motion drift between simulated
time and real time, compounding every tick. Exact compute cost wasn't isolated by code reading
alone (algorithm complexity looks small — ~289 grid cells x 20 pressure iters, 323 particles —
not obviously seconds-scale), but the fix doesn't require knowing it.

**Decision**: make `dt` self-correcting and remove the artificial delay, instead of chasing the
exact hotspot.
- `dt` per tick = measured wall-clock delta (`HAL_GetTick()`, 1ms resolution) since the previous
  tick, clamped to `[0.001f, 0.04f]` s (`MIN_DT_S`/`MAX_DT_S`, board-specific glue in `main.c`,
  same pattern as `MAX_GRAVITY_MPS2`). Lower bound avoids dt=0 on a same-millisecond tick; upper
  bound caps solver instability if one tick stalls (e.g. a UART print). Passed directly into
  `simulateFlipFluid` in place of `axelorScene.dt`; `scene.c`'s `dt` field is untouched (still the
  20ms reference value Stage 1/2 validated against).
- `HAL_Delay(20)` removed from the loop tail — loop runs back-to-back as fast as compute allows,
  per explicit user request. Safe because `ChargePlex`'s scan is TIM1+DMA-driven, fully decoupled
  from main-loop rate (`contracts/display-driver.md`) — no charlieplex code, call site, or call
  rate changes.
- Print throttling (`MPU6500_ReadAccel`'s two prints + the `Counter` print) switched from
  iteration-count-based (`printTick % 25`) to elapsed-time-based (`HAL_GetTick()` delta >= 500ms).
  Iteration-count throttling assumed ~20ms/iteration; at the now-variable, likely much faster loop
  rate it would otherwise print far more often than intended, reintroducing blocking UART time.
- No regression to the constitution's already-logged no-sleep-mode gap (Complexity Tracking):
  `HAL_Delay` was already a busy-wait (no WFI), so the CPU was already 100% busy during it: removing
  it doesn't change power posture, just removes dead time.

Build-verified: 0 errors, 0 warnings, both configs. On-hardware re-test of tilt latency is a new
task (see tasks.md).

## Summary of resolved unknowns (Round 9 additions)

| Unknown | Resolution |
|---|---|
| Which copied `flip_sim_c` files are actually firmware-portable | `flip_fluid.{c,h}`, `flip_utils.{c,h}`, `scene.{c,h}` only — `main.c`/`util.{c,h}` are Win32-only and excluded (Decision 18) |
| `malloc`/`calloc` on a bare-metal target with no heap setup | Converted to fixed-size `static` arrays sized to this project's one fixed tank configuration (~21 KB total, well under the 51.2 KB SRAM budget) (Decision 19) |
| Suspected stray `floor` (not `floorf`) call | False positive — was a comment match, not a real call; zero actual non-`f` `floor` calls exist (Decision 20, corrected) |
| Simulation tick rate vs. the existing 500ms print/blink loop | Main loop period shortened to 20ms to match `dt`; verbose UART prints throttled to every 25th iteration instead (Decision 21) |
| Risk of an unclamped acceleration spike destabilizing the solver | `gravity_x`/`gravity_y` magnitude-clamped in `axelor`'s glue code before reaching `simulateFlipFluid` (Decision 22) |
| How `cellColor` actually reaches the LED matrix | Copied directly into `cpFrameBuf[row][col]` (row-major index match confirmed exact, no remapping) — `ChargePlex_ApplyFrameBuffer`'s validate-or-reject gate and everything below it stays untouched (Decision 23) |
| `Scene.paused` defaults to `true` (Win32 "click Start" convention) | Forced `false` in `axelor`'s glue only; `scene.c`'s shared default is unchanged (Decision 24) |
