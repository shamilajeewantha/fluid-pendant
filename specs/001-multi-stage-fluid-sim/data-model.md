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
| `numParticles` / `maxParticles` | `int` | resized per research.md Decision 3 to fill the 16x15 domain |
| `fNumX`, `fNumY` | `int` | set to 16 and 15 respectively (or 15/16 depending on solver's row/col convention — verified during implementation against `update_cell_colors_from_types`) |
| `density`, `tankWidth`, `tankHeight`, `h`, `r` | `float` | unchanged formulas, recomputed for the new tank size |
| `cellColor` | `float*`, sized `16*15` | was hardcoded to `10*10`; becomes the source for DisplayFrame |
| `Scene.gravity_x` / `gravity_y` | `float` | **changed from `double`** per research.md Decision 2 |
| `Scene.numPressureIters` | `int` | default lowered from 100 (research.md Decision 1), tunable on hardware |

**Validation rules** (carried over from existing code, unchanged):
- `numParticles <= maxParticles`.
- Grid cells flagged solid (`s[i] == 0`) are never written by the pressure solver (existing rule).
- `flipRatio`, `overRelaxation`, `numParticleIters`/`numPressureIters` stay within the ranges
  already exposed by the existing JS/C scene defaults (no new bounds introduced).

## MotionInput

A directional force value driving the simulation's gravity, sourced from either a UI control
(Stages 1–2) or the live accelerometer (Stage 3).

| Field | Type | Notes |
|---|---|---|
| `x` | `float` | maps to `Scene.gravity_x` |
| `y` | `float` | maps to `Scene.gravity_y` |

**Validation rules**:
- Magnitude clamped to a maximum (prevents a violent shake from injecting an unbounded force into
  `integrate_particles`, per spec edge case "extreme acceleration spike").
- On Stage 3, dead-banded/low-pass filtered before being written (per research.md Decision 5) so
  single noisy samples don't visibly perturb the fluid.
- On missing/invalid sensor input, the last valid `MotionInput` is held rather than zeroed or
  treated as undefined (per spec edge case "accelerometer briefly fails").

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

## AxelorFrameBuffer (axelor, Round 8 — research.md Decision 17)

The firmware-local stand-in for `DisplayFrame`/`cellColor` until the physics core is ported onto
this board — same 240-slot indexing as the existing charlieplex `bsrr_buf`/`moder_buf` (one entry
per `(src,dst)` pin-pair slot, in `ChargePlex_BuildScanTable`'s existing enumeration order), but
owned by a pattern generator instead of `FluidSimState`, and consumed by a translation step instead
of being read directly by DMA.

| Field | Type | Notes |
|---|---|---|
| `frameBuf` | fixed-size array, 240 entries, 1 bit each (on/off) | which slots the *current* pattern marks "on"; refilled by the pattern generator, never read by DMA directly |
| pattern generator state | one persisted integer (sweep offset) | advances by a fixed step each refresh tick, wraps modulo 240 — no floats/trig needed (research.md Decision 17) |

**Validation rules**:
- Refreshed once per ~0.5s tick (reuses the existing main-loop cadence), **decoupled from** the
  DMA scan's own much faster, unchanged rate.
- Every refresh is translated into `moder_buf` only (`bsrr_buf` is written once at boot and never
  again, since pin identity per slot never changes — only on/off does) and re-validated by
  `ChargePlex_ValidateScanTable()` (relaxed to accept zero-output "off" slots as valid, alongside
  the existing exactly-2-output "on" slot rule) **before** being allowed to replace the live
  `moder_buf` — a malformed new frame is dropped (previous frame kept) rather than risked, per this
  board's no-current-limiting-resistor constraint already documented in tasks.md/plan.md history.
- This is the exact slot the eventual `DisplayFrame`/`cellColor` output is expected to fill once the
  physics core is ported — the pattern generator is a placeholder producer for the same consumer
  (the `moder_buf` translation step), not a separate mechanism that integration would need to
  replace.
