# Implementation Plan: Multi-Stage Fluid Simulation Pendant

**Branch**: `001-multi-stage-fluid-sim` | **Date**: 2026-06-21 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-multi-stage-fluid-sim/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Port the existing, already-working FLIP/PIC fluid simulation source (`final_project/flip_sim_c`,
paired with the visual reference `final_project/flip_sim_js`) onto this project's real hardware
target — an **STM32L431CC** ("axelor" board) driving a **16-row x 15-column charlieplexed LED
matrix** from **MPU-6500 IMU (accelerometer axes only)** input — without rewriting the underlying
physics formulas.
The physics core is parameter-tuned (fewer pressure-solver iterations, single-precision float
everywhere, grid/particle count resized to match the 16x15 display 1:1) to fit the MCU's real-time
budget, and the browser prototype and Windows simulator are both updated to the same 16x15
resolution so all three stages stay visually consistent before flashing hardware.

This plan corrects a hardware-target error discovered during planning: the project constitution
recorded **STM32F103C8T6** (no FPU) as the target MCU. The user confirmed the real target is
**STM32L431CC** (Cortex-M4F, has FPU) via
https://www.st.com/en/microcontrollers-microprocessors/stm32l431cc.html, which also matches
`stm_projects/axelor/axelor.ioc` (`Mcu.UserName=STM32L431CCTx`, `RCC.SYSCLKFreq_VALUE=80000000`).
The constitution has been amended (v1.0.0 → v2.0.0) to reflect this before this plan was written;
see the Constitution Check section below. A second correction followed in the same planning
session: the sensor is the **MPU-6500** (6-axis IMU, SPI), not the ADXL362 originally recorded —
only its accelerometer axes are used (gyroscope disabled to save power). The constitution was
amended again (v2.0.0 → v2.1.0) to reflect this.

**Round 2 (2026-06-21, post-implementation visual QA)**: after Stages 1–2 were implemented and run,
the user found four issues, each resolved as a further parameter/UI-only change (research.md
Decisions 7–10): (1) the solver grid's wall cells coincided exactly with the outermost ring of
display cells, so the left/right/bottom LEDs were structurally incapable of ever lighting — fixed
by padding the solver's internal grid one cell beyond the display on walled sides while keeping the
display/`cellColor` contract at exactly 16x15; (2) Stage 1 felt noticeably "lighter" than Stage 2 —
traced to `numPressureIters` being lowered to 20 in Stage 2 (Decision 1) but left at the original
100 in Stage 1, now matched; (3) Stage 2 had no cell-separator lines or row/column labels and an
oversized window — fixed via rendering-only changes (a smaller `CELL_SIZE`, cell borders, static
text labels), explicitly not touching any simulation-space scale value; (4) Stage 1's start/pause
was keyboard-only (`p`) with no visible control — added explicit Start/Pause buttons matching
Stage 2's existing UI pattern.

**Round 3 (2026-06-21, "match them both to real water")**: the user asked why the two stages still
felt like different fluids and asked for both to look like real water, constrained to stay
STM32L4-friendly (research.md Decision 11). Root cause: Stage 2's `dt` (1/120s, ~8.33ms) didn't
match its own `WM_TIMER` tick interval (20ms), so it played its own physics back at only ~42% of
real speed — slow-motion fluid reads as thick/syrupy. Fixed by setting `dt` to match the existing
20ms interval (same 50Hz tick rate, zero added compute — deliberately not the alternative of
raising the tick rate to match `dt`, which would cost 2.4x more pressure-solver work per second).
Verified stable via a headless 3000-step run at the new `dt` with the existing iteration count.
Stage 1 needed no change — its tick-interval target already approximated its own `dt`.

**Round 4 (2026-06-21, surface-cell flicker at rest)**: the user reported LEDs along the air/water
boundary blinking even with the gravity slider held still, and asked for the most robust,
lightweight, STM32L4-friendly fix, researched against current literature (research.md Decision
12). Root cause: both stages light an LED from a hard binary `cellType` flag that flips fully
whenever a jittering particle's position crosses one exact cell-boundary point — a step function
with all its sensitivity concentrated at a single point. Fixed by switching the display threshold
to a hysteresis check (two thresholds, with a dead zone between them) on `particleDensity`, a
continuous value both solvers already compute every tick for an unrelated purpose (pressure-solver
drift compensation) — so the fix costs one persisted 240-cell array and two comparisons per cell
per frame, with the solver itself untouched.

**Round 5 (2026-06-21, realistic display within a fixed 16x15 grid)**: the user asked for the most
realistic possible rendering of the fluid on the matrix, noting the underlying particles already
behave realistically — only the binary display was throwing that away — researched against real
reference projects (research.md Decision 13). Found this project's direct ancestor, mitxela's
"Fluid Simulation Pendant" (same STM32L4 family, same FLIP algorithm, same originally-assumed
ADXL362 sensor), which renders from per-cell particle density but achieves visual smoothness via a
much denser physical LED count plus optical diffusion — neither available to us at a fixed 16x15.
The available, equally lightweight lever is per-LED brightness: cells now render continuous
brightness directly from `particleDensity`/`particleRestDensity` (clamped 0-1) instead of any
binary decision, which **supersedes and removes** Round 4's hysteresis logic entirely (it was a
patch for a problem only binarization created). Forward-compatible with real hardware via cheap
bit-angle-modulation PWM in the existing DMA-driven charlieplex scan (documented only — Stage 3
remains deferred).

**Round 6 (2026-06-21, top display row never lights up)**: the user reported the topmost display
row never lights up, confirmed via a 20,000-step headless sweep (including violent/upward gravity)
showing 0.000000 brightness there under any condition — a hard structural exclusion, not a rare
event (research.md Decision 14). Root cause: an inherited off-by-one in the density-splat and
velocity-transfer neighbor clamp (`y1 = ... : fNumY-2` instead of `fNumY-1`), harmless in the
original tutorial's all-four-sides-walled design (that index was a wall cell there) but left that
row structurally unreachable once Round 2's Decision 7 deliberately opened the top wall — silently
undoing half of that earlier fix. Corrected the clamp's constant in both files/stages; no formula,
parameter, or algorithm changed, and `solve_incompressibility`'s separate exclusion of that row
from direct pressure correction was deliberately left alone (the correct way to represent a
free/open top surface). A separately reported leftmost-column motion at rest was investigated (no
code asymmetry found between left/right walls) and the user explicitly accepted it as genuine,
realistic settling motion rather than asking for a fix.

**Round 7 (2026-06-22, real hardware has no per-LED PWM — revert to binary display)**: the user
realized the real charlieplex hardware can't actually do per-LED brightness, confirmed against
this project's own `sample-charlieplexing` reference driver (`bsrr_buf[240]`/`moder_buf[240]` hold
exactly one on/off pattern per scan, not multiple brightness bit-planes) — Round 5's
forward-compatibility assumption (bit-angle-modulation PWM) would need restructuring that fixed,
single-buffer scan, which is more complexity than the user wants on the STM32L4 (research.md
Decision 15). Reverted Round 5's continuous brightness, but specifically by **reinstating** Round
4's hysteresis-on-density logic (not the original raw binary `cellType` flag) — same persisted
state and two threshold comparisons as before, so the binary output stays flicker-free without
costing anything new.

**Round 8 (2026-06-22, axelor Stage 3 bring-up begins)**: this plan's original Structure Decision
deferred Stage 3 entirely to a later, user-initiated phase. That phase has now started, directly in
`stm_projects/axelor` — the user independently brought the charlieplex DMA scan and MPU-6500 SPI
read path up to a working, on-hardware-confirmed state across the preceding sessions. This round
scopes the next two narrow pieces (research.md Decisions 16–17), **explicitly excluding** any
physics-core integration: (1) extend the existing UART accel telemetry to also print the value in
the exact units `MotionInput` will need (`m/s²`), verified on real hardware before any simulation
code exists to consume it; (2) replace the current always-everything-on bring-up test pattern with
a new, periodically-refreshing (~0.5s) frame buffer — the firmware-local stand-in for
`DisplayFrame`/`cellColor` — translated into the existing `moder_buf` only, with the DMA scan
mechanism itself left completely unchanged. All edits for this round are confined to
`stm_projects/axelor` per explicit user instruction.

## Technical Context

**Language/Version**: C11 for all three stages' simulation/firmware code; vanilla JavaScript
(ES2017+, no build step) for the Stage 1 browser prototype.

**Primary Dependencies**:
- Stage 1 (browser): none — plain DOM/Canvas, matches existing `flip_sim_js`.
- Stage 2 (Windows): Win32 API + Common Controls (already used by `final_project/flip_sim_c`),
  built with the existing MinGW `Makefile`.
- Stage 3 (firmware): STM32CubeIDE project using STM32L4xx HAL/CMSIS, modeled on
  `stm_projects/axelor` (SPI1 already configured for the accelerometer) and reusing the
  TIM+DMA-to-GPIO-`BSRR`/`MODER` charlieplexing driver pattern proven in
  `stm_projects/sample-charlieplexing`.

**Storage**: N/A — all state is in-memory/runtime; nothing is persisted across power cycles.

**Testing**: No formal unit test framework exists yet for the C core. Per constitution Principle
III, the ported physics module MUST stay host-buildable; this plan keeps `flip_fluid.c`/`flip_utils.c`/
`scene.c` free of any STM32 HAL include, which is itself what makes Stage 2 (the Windows build)
a continuous correctness check for Stage 3's physics. A lightweight host-side smoke test
(documented in `quickstart.md`) substitutes for a full unit-test suite at this scope.

**Target Platform**: Web browser (Stage 1) · Windows desktop, x86/x64 (Stage 2) ·
STM32L431CC bare-metal firmware via STM32CubeIDE (Stage 3).

**Project Type**: Multi-stage port — one shared physics core, three independently buildable host
environments (browser, Windows, embedded).

**Performance Goals**:
- LED display refresh ≥ 10 FPS (constitution Principle IV floor).
- Each simulation tick ≤ 50 ms on the STM32L431CC at 80 MHz.
- Tilt-to-visible-LED-response latency < 500 ms (spec SC-004).

**Constraints**:
- Flash ≤ 80% of 256 KB, RAM ≤ 80% of 64 KB on the STM32L431CC (constitution Principle IV).
- Single-precision `float` only in the physics core — no `double` (constitution Principle IV, v2.0.0).
- DMA-driven LED matrix refresh and SPI sensor transfer; interrupt-driven accelerometer reads;
  no polling loops (constitution Principle II / Hardware Constraints).
- Physics core MUST NOT include STM32 HAL headers or reference GPIO/SPI/timer registers
  (constitution Principle III; this is also spec FR-008/FR-009 and SC-006).

**Scale/Scope**: Single pendant device, single user, one display (16x15 = 240 addressable LEDs via
16-pin charlieplexing), one accelerometer. Simulation domain resized to 16x15 cells (from the
original ~10x10), particle count scaled proportionally from the original ~56 (see research.md
Decision 3).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle / Constraint | Status | Notes |
|---|---|---|
| I. Clean Code Quality | PASS | Porting preserves existing identifier/structure conventions; any new charlieplex pin-table code will use named constants, not magic numbers. |
| II. Low-Power C Development | PASS (planned) | Firmware will use STOP mode between simulation ticks once frame-rate budget is confirmed on hardware; DMA for display refresh and SPI keeps the CPU free to sleep. Power validation on real hardware is a Stage 3 task, not a plan-time gate. |
| III. Modular Architecture | PASS | `flip_fluid.c/.h`, `flip_utils.c/.h`, `scene.c/.h` are already HAL-free and will be ported unchanged in logic; new charlieplex driver and MPU-6500 driver are separate modules per contracts/. |
| IV. Performance Requirements | PASS (post-amendment) | Was failing under the old (wrong) constitution, which mandated fixed-point for a no-FPU chip that isn't the real target. Now aligned: float32 throughout, STM32L431CC has the FPU to support it. Iteration-count reduction (research.md Decision 1) is the lever for the 50ms/tick budget. |
| Hardware Constraints | PASS (post-amendment) | Target MCU corrected to STM32L431CC; display constraint (16 rows x 15 columns charlieplexed) already matched the spec and is now also recorded in the constitution itself. |
| Development Workflow | PASS (planned) | New STM32CubeIDE project will compile under `-Wall -Wextra`; on-device validation is a Stage 3 implementation task, tracked in tasks.md, not skipped here. |

No unjustified violations. Complexity Tracking table is not needed (empty).

## Project Structure

### Documentation (this feature)

```text
specs/001-multi-stage-fluid-sim/
├── plan.md              # This file (/speckit-plan command output)
├── research.md           # Phase 0 output (/speckit-plan command)
├── data-model.md          # Phase 1 output (/speckit-plan command)
├── quickstart.md          # Phase 1 output (/speckit-plan command)
├── contracts/             # Phase 1 output (/speckit-plan command)
│   ├── physics-core.md
│   ├── display-driver.md
│   └── sensor-driver.md
└── tasks.md               # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
final_project/flip_sim_js/         # Stage 1 — browser prototype (existing, EXTENDED)
├── flip_sim.html                  # grid updated from 8x8 demo cells to 16x15 to match final display
├── script.js                      # gravity-direction control kept as-is (matches accel-angle mapping)
└── flip.js                        # FLIP-style logic local to the prototype only (not shared w/ C)

final_project/flip_sim_c/          # Stage 2 — Windows simulator + canonical ported physics (existing, MODIFIED)
├── flip_fluid.c / flip_fluid.h     # physics core: GRID_X/GRID_Y resized 16x15, double->float params,
│                                   #   numPressureIters tuned down (research.md Decision 1 & 2)
├── flip_utils.c / flip_utils.h     # setupScene(): tank/particle sizing rescaled to fill 16x15 domain
├── scene.c / scene.h               # Scene struct/defaults — unchanged shape, dt/iteration defaults tuned
├── util.c / util.h, main.c         # Win32 display/UI glue — ONLY consumer of the 16x15 grid for
│                                   #   on-screen rendering; no physics changes here
└── Makefile

stm_projects/<new-pendant-firmware>/   # Stage 3 — STM32CubeIDE project, modeled on stm_projects/axelor
└── (DEFERRED — see note below; layout kept here only as forward design reference)
    ├── *.ioc, .project, .cproject         # generated from axelor.ioc (STM32L431CCTx, SPI1 for the IMU)
    ├── Core/Src/
    │   ├── physics/                       # flip_fluid.c, flip_utils.c, scene.c copied verbatim from
    │   │                                   #   final_project/flip_sim_c — NO HAL includes (contracts/physics-core.md)
    │   ├── display/                       # charlieplex_driver.c — TIM+DMA GPIO BSRR/MODER pattern adapted
    │   │                                   #   from stm_projects/sample-charlieplexing, scaled to 16 pins
    │   ├── sensor/                        # mpu6500_driver.c — interrupt-driven SPI1 read of the MPU-6500's
    │   │                                   #   accelerometer registers only (gyro disabled), adapted from
    │   │                                   #   this repo's existing adxl-362-sensor/basic-adxl-with-matrix
    │   │                                   #   driver structure (SPI read/interrupt pattern reused, register
    │   │                                   #   map and init sequence are MPU-6500-specific, see research.md)
    │   └── main.c                         # glue only: tick -> read accel -> step physics -> build
    │                                       #   DisplayFrame -> hand off to display driver
    └── Core/Inc/ (matching headers)
```

**Structure Decision**: Stage 1 and Stage 2 work (`final_project/`) from earlier rounds stands as
before. **Stage 3 has now begun (Round 8), narrowly scoped and confined entirely to
`stm_projects/axelor`** — no other stage's files change in this round. The originally-imagined
Stage 3 subdirectory layout (`physics/`, `display/`, `sensor/`) below is **not** how `axelor` is
actually structured (it's a flat, CubeMX-generated `Core/Src/main.c` with hand-written additions in
`USER CODE` regions) and is kept only as the long-term forward-reference shape; Round 8's two
pieces both land as additions inside `axelor/Core/Src/main.c` (and its paired `main.h`/
`stm32l4xx_it.c` where a SysTick hook is needed), not as new files in a not-yet-existing
`display/`/`sensor/` split. The physics core itself (`flip_fluid.c`/`flip_utils.c`/`scene.c`) is
still **not** copied into `axelor` in this round — that remains a later, separate step, and
`tasks.md` for this round contains no physics-core-integration tasks.

## Complexity Tracking

> Not applicable — no Constitution Check violations require justification.
