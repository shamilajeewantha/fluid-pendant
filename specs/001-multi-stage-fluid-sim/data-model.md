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
| `cells` | fixed-size array, 16 × 15, one entry per LED | binary on/off per spec Assumptions (no per-cell brightness) |

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
