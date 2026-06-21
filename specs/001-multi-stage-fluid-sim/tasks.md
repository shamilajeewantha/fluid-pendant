---

description: "Task list for Multi-Stage Fluid Simulation Pendant"
---

# Tasks: Multi-Stage Fluid Simulation Pendant

**Input**: Design documents from `/specs/001-multi-stage-fluid-sim/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md (all present)

**Scope of this task list**: **Stage 1 (browser prototype) and Stage 2 (Windows simulator) only.**
Stage 3 (the STM32 firmware project) is explicitly deferred — the user will initiate that
STM32CubeIDE project themselves as a separate, later phase. Stage 3 design work already done
(research.md Decisions 4–6, data-model.md, contracts/display-driver.md, contracts/sensor-driver.md)
is kept as forward reference; the tasks that would implement it are listed in the **Deferred**
section at the end of this file for re-scoping later, but are NOT part of this execution pass.

**Canonical source location**: `final_project/flip_sim_js/` and `final_project/flip_sim_c/`. The
byte-identical duplicates at the repo root (`flip_sim_js/`, `flip_sim_c/`) are intentionally left
untouched by every task below.

**Tests**: Not explicitly requested in spec.md. No automated test-writing tasks are included;
validation instead uses the manual scenarios already defined in `quickstart.md`.

**Organization**: Tasks are grouped by user story (US1 = browser prototype, US2 = Windows
simulator) per spec.md's priorities (P1, P2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (US1, US2)
- Exact file paths are included in every task

---

## Phase 1: Setup

**Purpose**: Confirm existing baselines still work before any edits.

- [ ] T001 [P] Build `final_project/flip_sim_c` with its existing `Makefile` and confirm `main.exe` runs unmodified (pre-port baseline; no code changes)
- [ ] T002 [P] Open `final_project/flip_sim_js/flip_sim.html` in a browser and confirm the existing 8x8 prototype still runs (pre-port baseline; no code changes)

**Checkpoint**: Baselines confirmed.

---

## Phase 2: Foundational — Port & Tune the Shared Physics Core

**Purpose**: Apply the parameter-tuning changes from research.md (Decisions 1–3) to the canonical
physics source in `final_project/flip_sim_c`. **User Story 2 depends on this phase** (it runs this
code directly). **User Story 1 does NOT depend on this phase** (the browser prototype has its own
independent logic) and may proceed in parallel with Phase 1.

**⚠️ CRITICAL**: US2 cannot start until this phase is complete.

- [ ] T003 In `final_project/flip_sim_c/flip_fluid.h` and `final_project/flip_sim_c/flip_fluid.c`, change `simulateFlipFluid`'s `gravity_x`/`gravity_y` parameters from `double` to `float` (research.md Decision 2); also change `Scene.gravity_x`/`gravity_y` from `double` to `float` in `final_project/flip_sim_c/scene.h` and update `scene_default()` in `final_project/flip_sim_c/scene.c` accordingly
- [ ] T004 In `final_project/flip_sim_c/flip_fluid.c`, resize the solver's output grid constants from `#define GRID_X 10` / `#define GRID_Y 10` to 16 and 15 respectively (research.md Decision 3), and update `update_cell_colors_from_types`'s loop bounds and the `cellColor` buffer size accordingly (depends on T003 — same file)
- [ ] T005 In `final_project/flip_sim_c/flip_utils.c`'s `setupScene()`, rescale `tankWidth`/`tankHeight`/`res` and the hexagonal-packing `numX`/`numY` formula so `fNumX`/`fNumY` come out to 16x15 (1:1 with the display) and `maxParticles` scales proportionally (research.md Decision 3); update the `f->cellColor` allocation from `10 * 10` to `16 * 15` (depends on T004)
- [ ] T006 In `final_project/flip_sim_c/scene.c`'s `scene_default()`, lower `numPressureIters` from `100` to `20` (research.md Decision 1) — the SOR/Gauss-Seidel algorithm in `solve_incompressibility` itself is NOT changed, only this iteration-count parameter (depends on T003 — same file)
- [ ] T007 In `final_project/flip_sim_c/util.c`'s `UpdateGridFromFluid`, update the `cellColor` indexing to address the new 16x15 buffer instead of the old hardcoded 10x10 layout (depends on T005)

**Checkpoint**: Physics core is ported (float-only) and tuned (16x15, fewer pressure iterations) — ready for US2.

---

## Phase 3: User Story 1 - Quick Visual Prototype in the Browser (Priority: P1) 🎯 MVP

**Goal**: A browser-based fluid prototype, resized to the final 16x15 display resolution, that
lets a developer change simulated gravity direction and see the fluid visibly respond.

**Independent Test**: Open `final_project/flip_sim_js/flip_sim.html` with nothing else built;
change the gravity-direction control and confirm the fluid visibly redirects within a few frames.

### Implementation for User Story 1

- [ ] T008 [P] [US1] In `final_project/flip_sim_js/flip_sim.html`, resize the `.grid` CSS (`grid-template-columns`/`grid-template-rows`) from `repeat(8, 50px)` to 16 rows x 15 columns
- [ ] T009 [P] [US1] In `final_project/flip_sim_js/flip.js`, update the simulation grid resolution constants to 16x15 so the prototype's internal grid matches the new display size
- [ ] T010 [US1] In `final_project/flip_sim_js/script.js`, update the grid-construction/render loop to build and update a 16x15 cell grid instead of 8x8, reading from the resized `flip.js` state (depends on T008, T009)
- [ ] T011 [US1] Manually validate per `quickstart.md` Stage 1 scenario: open `final_project/flip_sim_js/flip_sim.html`, confirm the 16x15 grid animates, and confirm the gravity-direction control visibly redirects flow within ~1 second (spec SC-001) (depends on T010)

**Checkpoint**: User Story 1 is fully functional and independently testable — this is the MVP slice.

---

## Phase 4: User Story 2 - Desktop Simulator for Pre-Hardware Validation (Priority: P2)

**Goal**: The Windows simulator runs the ported/tuned physics core unchanged in logic, rendering
at the same 16x15 resolution as the final hardware.

**Independent Test**: Build and run `final_project/flip_sim_c` alone; confirm it reproduces fluid
motion using the reused (not rewritten) physics source, at 16x15 resolution.

**Depends on**: Phase 2 (Foundational) must be complete.

### Implementation for User Story 2

- [ ] T012 [US2] In `final_project/flip_sim_c/util.h`, change `#define SIZE 8` to reflect the new 16-row x 15-column display, and update the window-sizing math in `final_project/flip_sim_c/main.c` (`SIZE * CELL_SIZE + 200` usages) for the non-square 16x15 grid
- [ ] T013 [US2] In `final_project/flip_sim_c/util.c`'s `DrawGrid` and `UpdateGridFromFluid`, remove the old "extract inner 8x8 from a 10x10" cropping logic — the solver grid is now 1:1 with the 16x15 display, so every solver cell maps directly to one displayed cell (depends on T007, T012)
- [ ] T014 [US2] Rebuild `final_project/flip_sim_c` with `make` and confirm it compiles with no new warnings and `main.exe` runs at the new 16x15 resolution (depends on T013)
- [ ] T015 [US2] Manually validate per `quickstart.md` Stage 2 scenario: run unpaused for several minutes, move the gravity-angle trackbar and confirm consistent flow-direction response (spec SC-002, SC-003), and perform the host-side smoke check (temporarily log `cellColor` and confirm no NaN/inf values appear) (depends on T014)

**Checkpoint**: User Stories 1 AND 2 both work independently; the physics core is validated on a host PC, ready for the (separately-initiated) Stage 3 firmware phase to copy.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency checks across the two in-scope stages.

- [ ] T016 [P] Update `final_project/flip_sim_c/README.md` with the new 16x15 resolution and a note describing the physics/display module boundary (`contracts/physics-core.md`), so the Stage 3 phase has an accurate starting point when it begins
- [ ] T017 Run the `quickstart.md` Stage 1 and Stage 2 validation passes back-to-back and record the result (depends on T011, T015)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: No dependency on Setup completion, but sequenced after it for
  clarity — **blocks US2 only**
- **User Story 1 (Phase 3)**: Depends only on Setup (T002) — can run **in parallel with Phase 2**
- **User Story 2 (Phase 4)**: Depends on Foundational (Phase 2) completion
- **Polish (Phase 5)**: Depends on both user stories being complete

### Within Phase 2 (Foundational)

T003 → T004 → T006 are sequential edits to the same files (`flip_fluid.c`, `scene.c`); T005
depends on T004's grid resize; T007 depends on T005. This phase is inherently sequential — it is
a small, tightly-coupled set of edits to a handful of existing files, not a set of independent
modules.

### Parallel Opportunities

- T001 and T002 (Phase 1) can run in parallel
- Phase 3 (User Story 1) can run in parallel with Phase 2 (Foundational) — different
  files/languages, no shared dependency
- T008 and T009 (Phase 3) can run in parallel

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T002)
2. Complete Phase 3: User Story 1 (T008–T011)
3. **STOP and VALIDATE**: open the browser prototype, confirm SC-001
4. This is demoable on its own — no C toolchain required

### Incremental Delivery

1. Setup (Phase 1) → Foundational (Phase 2) can start right after; US1 (Phase 3) can run in
   parallel with Foundational
2. Foundational (Phase 2) done → User Story 2 (Phase 4) → validate independently → this is the
   pre-hardware checkpoint the whole project exists to provide
3. Phase 5 (Polish) once both stages are validated
4. **Stage 3 (firmware) begins as a separate, later phase**, initiated by the user — see Deferred
   section below for the task list to re-scope at that time

### Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps each task to US1/US2 for traceability; Setup/Foundational/Polish tasks carry
  no story label per the task-format rules
- Tests were not requested for this feature; validation is via the manual scenarios already
  written in `quickstart.md`, referenced directly from the relevant tasks above
- Commit after each task or logical group
- Several Foundational tasks intentionally stay narrow even though they touch overlapping files
  (e.g., T003 vs. T004 vs. T006), so a mistake in one tuning decision (e.g., iteration count) can
  be isolated and reverted without undoing the others

---

## Deferred — Stage 3 (Firmware), NOT part of this execution pass

The user will initiate the STM32CubeIDE project themselves as a separate, later phase. The task
breakdown below reflects the Stage 3 design already captured in research.md/data-model.md/
contracts/, kept here only so that future phase has a ready starting point to re-scope (e.g. the
file paths and the "new project" assumption should be re-checked against whatever the user has
actually set up by then) — **do not execute these now**.

- Create the new STM32CubeIDE project (cloned from `stm_projects/axelor`) with `Core/Src/physics/`, `Core/Src/display/`, `Core/Src/sensor/` subdirectories
- Copy `flip_fluid.c`/`.h`, `flip_utils.c`/`.h`, `scene.c`/`.h` verbatim (no STM32 HAL includes) from `final_project/flip_sim_c/` into the new project's `Core/Src/physics/` / `Core/Inc/physics/`, per `contracts/physics-core.md`
- Implement the 16-pin charlieplex addressing lookup table and the TIM+DMA GPIO `BSRR`/`MODER` scan driver in `Core/Src/display/charlieplex_driver.c`, adapted from `stm_projects/sample-charlieplexing` (research.md Decision 4, `contracts/display-driver.md`)
- Implement MPU-6500 register init (Low-Power Accelerometer Mode, gyro disabled, ±2g full-scale, Data Ready interrupt) and the interrupt-driven SPI1 read + dead-band filter in `Core/Src/sensor/mpu6500_driver.c`, per `contracts/sensor-driver.md` and research.md Decision 5
- Implement `main.c` glue: read `MotionInput` → write `Scene.gravity_x/y` → `simulateFlipFluid` → build `DisplayFrame` → hand off to the display driver; add `STOP`-mode power management between ticks (constitution Principle II)
- Build in STM32CubeIDE and confirm Flash/RAM usage ≤ 80% of 256 KB / 64 KB (constitution Principle IV)
- On-hardware validation per `quickstart.md` Stage 3 scenarios (neutral boot pattern, tilt latency < 0.5s, settles when still, ≥10 min untethered operation) and supply-current measurement against the MPU-6500's documented power model (`contracts/sensor-driver.md`)
- Grep the copied physics files for any STM32 HAL include/register reference and confirm none exist (constitution Principle III)
