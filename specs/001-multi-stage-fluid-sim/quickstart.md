# Quickstart: Multi-Stage Fluid Simulation Pendant

Validation scenarios for each stage, in dependency order. Each stage should be validated before
moving to the next — that's the entire point of staging this project (see spec.md User Stories).

## Stage 1 — Browser prototype

**Prerequisites**: any modern browser, no build step.

```text
open final_project/flip_sim_js/flip_sim.html
```

**Expected outcome**: the fluid grid (resized to 16x15 per research.md Decision 3) renders and
animates; moving the gravity-direction control visibly redirects the fluid within a few frames
(spec SC-001). This validates spec User Story 1 in isolation.

**Round 2 checks** (research.md Decisions 7–10):
- Use the visible **Start**/**Pause** buttons, not just the `p` key, to control the simulation.
- Row/column index labels are visible around the grid.
- Over time (try several gravity-angle settings), fluid visibly reaches the left, right, and
  bottom edge cells — those are no longer permanently dead/unlit. The very top row may stay sparse
  most of the time — that's expected, since this control never points gravity upward.
- The fluid's overall "weight"/responsiveness should feel comparable to Stage 2, not noticeably
  lighter (pressure-iteration count now matches).

## Stage 2 — Windows simulator

**Prerequisites**: MinGW (`gcc`) on PATH, on Windows.

```text
cd final_project/flip_sim_c
make
./main.exe
```

**Expected outcome**: a window opens showing the same FLIP fluid simulation, now rendered at the
16x15 grid resolution (matching the final LED matrix 1:1, research.md Decision 3). Moving the
gravity-angle trackbar changes the fluid's flow direction consistently with the physics model
(spec SC-002, SC-003 — confirms the ported physics core, not a rewrite, drives this view).

**Host-side smoke check** (substitutes for a formal test suite at this scope, per plan.md
Testing section): run with the simulation unpaused for several minutes and confirm the particle
positions/cell colors never produce `NaN`/`inf` (e.g., temporarily log `cellColor` and grep for
non-finite values) — this is the cheapest way to catch a parameter-tuning mistake (e.g., too few
pressure iterations causing divergence) before ever touching hardware.

**Round 3 check** (research.md Decision 11): time how long it takes a dropped/tilted blob of
fluid to fall and settle — it should look like real-time water motion, not slow motion. (Already
verified headlessly: 3000 steps at the corrected `dt` produced no instability.)

**Round 4 check** (research.md Decision 12, reinstated by Round 7 after a Round 5 detour — see
below): hold the gravity slider still for 10+ seconds after the fluid settles. The LEDs along the
air/water boundary should hold a stable pattern — no per-frame blinking — while still updating
promptly if the slider is then moved again.

**Round 5 check** (research.md Decision 13 — **reverted by Round 7**, kept here only as history):
this round briefly rendered a continuous brightness gradient at the surface instead of a hard
black/green line; it was reverted once it was confirmed the real charlieplex hardware can't do
per-LED PWM. Cells are binary again as of Round 7.

**Round 6 check** (research.md Decision 14): tilt/shake enough to send fluid toward the top of the
tank — the topmost display row ("row 1") should now be able to light up like any other row, rather
than staying permanently black regardless of how the fluid moves.

**Round 7 check** (research.md Decision 15): cells should be back to a hard on/off appearance
(matching the real hardware's binary charlieplex capability), and the Round 4 check above (no
per-frame blinking at rest) should still hold — confirming the hysteresis-on-density binary output
didn't bring back the original flicker.

**Round 2 checks** (research.md Decisions 7, 9):
- Cell separator lines are visible between cells, and row/column index labels are visible.
- The window no longer fills most of the screen (smaller on-screen `CELL_SIZE`).
- Fluid visibly reaches the left, right, and bottom edge cells over time/different gravity angles
  — those are no longer permanently dead/unlit (the solver's internal grid is now padded one cell
  beyond the display on walled sides; the display itself is still exactly 16x15).

## Stage 3 — Firmware on the axelor board (STM32L431CC)

> Round 8 (2026-06-22) began this stage, narrowly scoped to the two checks below — see plan.md's
> Structure Decision. Physics-core integration (the rest of this section, below the Round 8
> checks) remains a separate, later phase not yet started.

**Prerequisites**: STM32CubeIDE, the axelor board wired to its MPU-6500 (SPI, accelerometer axes
only) and the 16-pin charlieplexed LED matrix, ST-Link or equivalent programmer/debugger.

**Round 8 checks** (research.md Decisions 16–17; build/flash `stm_projects/axelor` per its own
README/CubeIDE project, then watch the UART at 115200 baud):

1. **Accel telemetry (Decision 16)**: with the board still/level, confirm the UART prints both the
   existing raw/mg accel values and the new `m/s²`-converted line, and that the `m/s²` value is
   close to what's expected for the board's current orientation (one axis near ±9.81, the other
   two near 0, for whichever axis is currently "down"). Tilt the board four ways (each in-plane
   direction) and confirm the printed `m/s²` values change sign/magnitude consistently with each
   tilt — this is how the MPU-6500-axis-to-display-plane mapping (still unresolved, see
   contracts/sensor-driver.md Round 8 status) gets resolved empirically. No simulation code reads
   these values yet (NO SIM INTEGRATION this round) — this is a UART-only check.
2. **Refreshing placeholder pattern (Decision 17)**: confirm the LED matrix no longer shows the
   prior "everything on, unchanging" bring-up pattern, and instead shows a pattern that visibly
   changes roughly every 0.5 second, with different LEDs lit across consecutive refreshes (the
   "sweeping window" — see research.md Decision 17). Confirm via UART that
   `ChargePlex_ValidateScanTable()`-style validation is still running on each new frame (no
   `[CHARLIEPLEX] FAULT` lines during normal operation). This pattern is not yet spatially
   meaningful (no confirmed slot→(row,col) mapping) — that it sweeps smoothly and covers the whole
   matrix over time is the thing being validated here, not its shape.

**Remaining Stage 3 work (not part of Round 8, prerequisites for the checks below)**: port
`flip_fluid.c`/`flip_utils.c`/`scene.c` onto `axelor`, replace the Round 8 placeholder pattern
generator with a real `DisplayFrame` producer from `cellColor`, and resolve the
`LedMatrixAddressing` slot→(row,col) mapping (contracts/display-driver.md Round 8 status) so that
mapping is geometrically meaningful.

```text
# In STM32CubeIDE: File > Open Projects from File System... > select the new firmware project
# (cloned from stm_projects/axelor per plan.md Structure Decision)
# Build, then Run > Debug (or flash via ST-Link Utility) to the board
```

**Expected outcome** (validates spec User Story 3 and SC-004/SC-005):
1. On power-up, before the first accelerometer reading, the LED matrix shows a defined neutral
   resting pattern (spec edge case) — not garbage.
2. Tilting the board visibly redirects the displayed fluid pattern within well under half a
   second (spec SC-004).
3. Holding the board still lets the fluid settle into a stable pattern rather than oscillating
   forever (spec acceptance scenario, User Story 3).
4. The device runs continuously for at least 10 minutes of mixed handling (tilt, shake, rest)
   with no frozen/corrupted display and no host PC connected (spec SC-005) — confirm by
   disconnecting the debugger/USB after flashing and observing on battery or bench power alone.

**Resource budget check** (constitution Principle IV gate, required before merge): after building,
check the linker map / build console output for Flash and RAM usage and confirm both are ≤ 80% of
256 KB flash / 64 KB SRAM respectively.

See `contracts/physics-core.md`, `contracts/display-driver.md`, and `contracts/sensor-driver.md`
for the per-module interface each layer must respect while implementing this stage.
