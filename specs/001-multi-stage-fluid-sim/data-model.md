# Data Model: Multi-Stage Fluid Simulation Pendant

Entities derived from spec.md's Key Entities section, made concrete against the existing
`final_project/flip_sim_c` source (per research.md Decisions 2–3: float32 only, 16x15 1:1 grid).

## FluidSimState

The physics core's live state. Maps directly to the existing `FlipFluid` struct
(`flip_fluid.h`) plus the `Scene` struct (`scene.h`) that wraps it — both are ported unchanged in
shape; only field *types* and the default tuning values they're initialized with change.

| Field (existing struct) | Type (after port) | Notes |
|---|---|---|
| `particlePos` | `float*` (x,y pairs) | unchanged |
| `s` | `float*` | per-cell solid/fluid flag, unchanged |
| `numParticles` / `maxParticles` | `int` | resized per research.md Decision 3 to fill the 16x15 domain — confirmed Round 9: `setupScene()`'s formula yields `maxParticles≈323` for this tank's fixed parameters |
| `fNumX`, `fNumY` | `int` | **confirmed Round 9** (not 16/15 directly): the *solver's* padded internal grid is `PAD_FNUM_X=17` × `PAD_FNUM_Y=17`; the *display* grid (`GRID_X=15` columns × `GRID_Y=16` rows) is one cell smaller on the walled sides per research.md Decision 7 — `update_cell_colors_from_types()` crops the padded grid down to the display grid when producing `cellColor` |
| `density`, `tankWidth`, `tankHeight`, `h`, `r` | `float` | unchanged formulas, recomputed for the new tank size |
| `cellColor` | `float*`, sized `GRID_X*GRID_Y` (15×16=240) | row-major, `index = row*GRID_X + col` (confirmed Round 9 by reading `update_cell_colors_from_types`) — directly matches `AxelorFrameBuffer.cpFrameBuf[row][col]`'s shape with no remapping (research.md Decision 23) |
| `Scene.gravity_x` / `gravity_y` | `float` | **changed from `double`** per research.md Decision 2 |
| `Scene.numPressureIters` | `int` | default lowered from 100 (research.md Decision 1), tunable on hardware |

**Validation rules** (carried over from existing code, unchanged):
- `numParticles <= maxParticles`.
- Grid cells flagged solid (`s[i] == 0`) are never written by the pressure solver (existing rule).
- `flipRatio`, `overRelaxation`, `numParticleIters`/`numPressureIters` stay within the ranges
  already exposed by the existing JS/C scene defaults (no new bounds introduced).
- **Round 9 (axelor only)**: all dynamic allocation (`malloc`/`calloc` in `flip_fluid.c`/
  `flip_utils.c`) is replaced with fixed-size `static` arrays sized to this tank's fixed
  `maxParticles`/grid dimensions (research.md Decision 19) — this project has exactly one tank
  configuration, so nothing requires runtime-variable sizing, and removing the heap removes an
  entire class of embedded-specific risk (allocation failure, fragmentation) this project has no
  use for.

## MotionInput

A directional force value driving the simulation's gravity, sourced from either a UI control
(Stages 1–2) or the live accelerometer (Stage 3).

| Field | Type | Notes |
|---|---|---|
| `x` | `float` | maps to `Scene.gravity_x` |
| `y` | `float` | maps to `Scene.gravity_y` |

**Validation rules**:
- Magnitude clamped to a maximum of **19.62 m/s²** (the MPU-6500's own ±2g full-scale range,
  confirmed via spec.md Clarifications Session 2026-06-23) — prevents a violent shake from
  injecting an unbounded force into `integrate_particles`, per spec edge case "extreme acceleration
  spike". **Implemented Round 9** in `axelor`'s glue code (research.md Decision 22) —
  `accelX_mps2`/`accelY_mps2` are clamped before being passed as `simulateFlipFluid`'s
  `gravity_x`/`gravity_y`, since the physics core
  itself (`integrate_particles`) must stay an unmodified, verbatim port (`contracts/physics-core.md`).
- On Stage 3, dead-banded/low-pass filtered before being written (per research.md Decision 5) so
  single noisy samples don't visibly perturb the fluid. **Still not implemented as of Round 9** —
  same open gap `contracts/sensor-driver.md`'s Round 8 status already logged; the magnitude clamp
  above is the minimum needed to satisfy the shake-stability edge case today, not a substitute for
  this filtering.
- On missing/invalid sensor input, the last valid `MotionInput` is held rather than zeroed or
  treated as undefined (per spec edge case "accelerometer briefly fails"). **Not yet implemented**
  — the current polled read has no failure-detection path at all (no SPI error handling beyond the
  status print); still open work alongside the interrupt-driven sensor contract.

## DisplayFrame

The derived 16x15 grid produced each tick from `FluidSimState.cellColor`, representing what the
LED matrix should currently show.

| Field | Type | Notes |
|---|---|---|
| `cells` | fixed-size array, 16 × 15, one entry per LED | binary on/off per cell (research.md Decision 15, reverting Decision 13's continuous brightness — real charlieplex hardware has no per-LED PWM) — derived via a hysteresis threshold on `particleDensity / particleRestDensity`, not a raw step-function flag |

**Validation rules**:
- Dimensions are fixed at 16 × 15 — never resized at runtime.
- Produced only from `FluidSimState.cellColor` (existing `update_cell_colors_from_types` logic,
  just re-targeted to 16x15 constants) — no other code path may write a DisplayFrame, preserving
  the physics/display separation (constitution Principle III; spec FR-008, SC-006).

## LedMatrixAddressing

The physical charlieplex addressing scheme: how a `DisplayFrame` cell maps to a (row-pin,
col-pin) drive pattern on the 16 GPIO pins.

| Field | Type | Notes |
|---|---|---|
| `pin count` | `16` (fixed) | per spec hardware constraint |
| `addressable LEDs` | up to `16 * 15 = 240` | classic charlieplexing: each pin pairs with each of the other 15 |
| mapping table | `DisplayFrame` index → (drive-high pin, drive-low pin) | generated/static lookup, lives entirely in the display driver module — physics core never sees pin numbers (constitution Principle III) |

**Validation rules**:
- Exactly one of the 16 pins may be actively driven high and one driven low at any instant during
  a scan step (true charlieplexing constraint); all others held Hi-Z.
- The mapping table is the *only* place `DisplayFrame` indices are translated to physical pins —
  it must not leak into `physics/` or `sensor/` modules.

## AccelTelemetry (axelor, Round 8 — research.md Decision 16)

A firmware-local, UART-only diagnostic projection of one `MotionInput`-shaped reading at each of
its conversion stages, printed side by side so an axis/sign/scale mistake is visible against a
known tilt before any simulation code exists to consume it. **Not** itself part of `MotionInput`
or written into any `Scene` field — purely observational at this stage.

| Field | Type | Notes |
|---|---|---|
| `accelX_raw`/`Y`/`Z` | `int16_t` | existing — two's-complement register value, unchanged |
| `accelX_mg`/`Y`/`Z` | `long` (milli-g) | existing — `raw * 1000 / 16384` (±2g range) |
| `accelX_mps2`/`Y`/`Z` | `float` (m/s²) | **new this round** — `(mg / 1000.0f) * 9.81f`; same unit/scale `MotionInput.x`/`y` and `Scene.gravity_x`/`y` already use |

**Validation rules**:
- Printed every existing 500ms read tick alongside the raw/mg values already printed — no new read
  cadence, no new sensor mode.
- Read-only with respect to simulation state: this round writes no `Scene`/`FlipFluid` field from
  any of these values (NO SIM INTEGRATION, per explicit user instruction).

## AxelorFrameBuffer (axelor, Round 8 — research.md Decision 17; producer replaced Round 9 — Decision 23)

The firmware-local stand-in for `DisplayFrame`/`cellColor` — shaped as an actual `[16][15]`
`[row][col]` matrix (revised from the original flat 240-entry shape once the board's real
row/column wiring convention was confirmed — every PB pin takes a turn as one row's source pin,
the other 15 as that row's sink pins), matching `FlipFluid.cellColor`'s own row-major shape
exactly. As of Round 9, this is filled directly from the real ported simulation's output instead of
a placeholder pattern generator — but the buffer's role in the architecture, and everything
downstream of it, is unchanged from Round 8.

| Field | Type | Notes |
|---|---|---|
| `cpFrameBuf` | `uint8_t[16][15]` | which `[row][col]` cells the *current* producer marks "on"; never read by DMA directly |
| producer (Round 8) | pattern generator, one persisted row index | placeholder: lit exactly one row per ~0.5s tick, advancing/wrapping mod 16 — superseded below |
| producer (Round 9) | `FlipFluid.cellColor[row*GRID_X+col] > 0.5f` | the real simulation's binary per-cell output (research.md Decision 23), copied in once per ~20ms simulation tick |

**Validation rules**:
- Refreshed once per simulation tick (Round 8: ~0.5s reusing the main-loop cadence; Round 9: ~20ms,
  matching `Scene.dt` — research.md Decision 21), **decoupled from** the DMA scan's own much
  faster, unchanged rate.
- Every refresh is translated into `moder_buf` only (`bsrr_buf` is written once at boot and never
  again, since pin identity per slot never changes — only on/off does) and re-validated by
  `ChargePlex_ValidateScanTable()` (relaxed to accept zero-output "off" slots as valid, alongside
  the existing exactly-2-output "on" slot rule) **before** being allowed to replace the live
  `moder_buf` — a malformed new frame is dropped (previous frame kept) rather than risked, per this
  board's no-current-limiting-resistor constraint already documented in tasks.md/plan.md history.
  **This validate-or-reject gate, and everything below it, is unchanged by the Round 9 producer
  swap** — the real simulation is held to the exact same boundary the placeholder pattern generator
  was, with no new path to influence `moder_buf`/`bsrr_buf`/hardware registers directly.
- This was the exact slot the `DisplayFrame`/`cellColor` output was expected to fill once the
  physics core was ported (stated in Round 8) — Round 9 confirms that prediction held with no
  rework needed to the consumer side; only the producer changed.
