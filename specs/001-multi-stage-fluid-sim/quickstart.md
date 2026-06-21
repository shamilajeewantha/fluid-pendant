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

**Round 4 check** (research.md Decision 12, superseded by Round 5 — see below): hold the gravity
slider still for 10+ seconds after the fluid settles. The LEDs along the air/water boundary should
hold a stable pattern — no per-frame blinking — while still updating promptly if the slider is
then moved again.

**Round 5 check** (research.md Decision 13): cells should now show a visible brightness gradient
at the fluid surface (dimmer near the edge of the water, brighter toward the interior) instead of
a hard black/green line — the boundary should look like a graded edge, not a binary step.

**Round 6 check** (research.md Decision 14): tilt/shake enough to send fluid toward the top of the
tank — the topmost display row ("row 1") should now be able to light up like any other row, rather
than staying permanently black regardless of how the fluid moves.

**Round 2 checks** (research.md Decisions 7, 9):
- Cell separator lines are visible between cells, and row/column index labels are visible.
- The window no longer fills most of the screen (smaller on-screen `CELL_SIZE`).
- Fluid visibly reaches the left, right, and bottom edge cells over time/different gravity angles
  — those are no longer permanently dead/unlit (the solver's internal grid is now padded one cell
  beyond the display on walled sides; the display itself is still exactly 16x15).

## Stage 3 — Firmware on the axelor board (STM32L431CC) — DEFERRED

> This stage is a separate, later phase that the user will initiate themselves; it is kept here
> only as forward reference. It is not part of the current `tasks.md` execution pass (see that
> file's "Deferred — Stage 3" section).

**Prerequisites**: STM32CubeIDE, the axelor board wired to its MPU-6500 (SPI, accelerometer axes
only) and the 16-pin charlieplexed LED matrix, ST-Link or equivalent programmer/debugger.

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
