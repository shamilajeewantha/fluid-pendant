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

**Build/run via Docker**: both stages are containerized for reproducible build/run, without
modifying any existing source file (only new `Dockerfile`s are added):
- `final_project/flip_sim_c/Dockerfile` cross-compiles the existing Win32 GUI app with
  `mingw-w64` inside a Linux container; the resulting `main.exe` is a real Windows PE binary,
  written straight onto the host via a bind mount, and is run on Windows exactly as before —
  Docker is only the build environment, never the runtime, for this native GUI app.
- `final_project/flip_sim_js/Dockerfile` serves the static prototype via `nginx:alpine`.

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

**Purpose**: Confirm existing baselines still work, and stand up the Docker build/run environment.

- [x] T001 [P] Build `final_project/flip_sim_c` with its existing `Makefile` and confirm `main.exe` runs unmodified (pre-port baseline; no code changes)
- [x] T002 [P] Open `final_project/flip_sim_js/flip_sim.html` in a browser and confirm the existing 8x8 prototype still runs (pre-port baseline; no code changes)
- [x] T003 [P] Create `final_project/flip_sim_c/Dockerfile` (Debian + `mingw-w64` + `make`); verify `docker build -t flip-sim-c-builder .` then `docker run --rm -v "$(pwd):/src" flip-sim-c-builder CC=x86_64-w64-mingw32-gcc` produces a valid `PE32+ ... MS Windows (GUI), x86-64` `main.exe` on the host with zero changes to any `.c`/`.h`/`Makefile` — **done this session, verified working**
- [x] T004 [P] Create `final_project/flip_sim_js/Dockerfile` (`nginx:alpine` serving the folder as-is); verify `docker build -t flip-sim-js-server .` then `docker run -p 8080:80 flip-sim-js-server` serves `flip_sim.html` with HTTP 200 — **done this session, verified working**

**Checkpoint**: Baselines confirmed; Docker build (C) and serve (JS) environments proven working.

---

## Phase 2: Foundational — Port & Tune the Shared Physics Core

**Purpose**: Apply the parameter-tuning changes from research.md (Decisions 1–3) to the canonical
physics source in `final_project/flip_sim_c`. **User Story 2 depends on this phase** (it runs this
code directly). **User Story 1 does NOT depend on this phase** (the browser prototype has its own
independent logic) and may proceed in parallel with Phase 1.

**⚠️ CRITICAL**: US2 cannot start until this phase is complete.

- [x] T005 In `final_project/flip_sim_c/flip_fluid.h` and `final_project/flip_sim_c/flip_fluid.c`, change `simulateFlipFluid`'s `gravity_x`/`gravity_y` parameters from `double` to `float` (research.md Decision 2); also change `Scene.gravity_x`/`gravity_y` from `double` to `float` in `final_project/flip_sim_c/scene.h` and update `scene_default()` in `final_project/flip_sim_c/scene.c` accordingly — *(scene.h/scene.c were already `float`; only `flip_fluid.h`/`flip_fluid.c`'s function parameters and the internal `integrate_particles` helper needed the `double`→`float` change)*
- [x] T006 In `final_project/flip_sim_c/flip_fluid.c`, resize the solver's output grid constants from `#define GRID_X 10` / `#define GRID_Y 10` to 16 and 15 respectively (research.md Decision 3), and update `update_cell_colors_from_types`'s loop bounds and the `cellColor` buffer size accordingly (depends on T005 — same file) — *(GRID_X=15 columns, GRID_Y=16 rows, per the `dst = y*GRID_X + x` row-major convention already in `update_cell_colors_from_types`; promoted to `flip_fluid.h` so `flip_utils.c` can share the same constants)*
- [x] T007 In `final_project/flip_sim_c/flip_utils.c`'s `setupScene()`, rescale `tankWidth`/`tankHeight`/`res` and the hexagonal-packing `numX`/`numY` formula so `fNumX`/`fNumY` come out to 16x15 (1:1 with the display) and `maxParticles` scales proportionally (research.md Decision 3); update the `f->cellColor` allocation from `10 * 10` to `16 * 15` (depends on T006) — *(fNumX/fNumY are now assigned directly from GRID_X/GRID_Y rather than re-derived via a fragile floor-division round-trip; particle count grew from ~56 to ~252, proportional to the larger domain)*
- [x] T008 In `final_project/flip_sim_c/scene.c`'s `scene_default()`, lower `numPressureIters` from `100` to `20` (research.md Decision 1) — the SOR/Gauss-Seidel algorithm in `solve_incompressibility` itself is NOT changed, only this iteration-count parameter (depends on T005 — same file)
- [x] T009 In `final_project/flip_sim_c/util.c`'s `UpdateGridFromFluid`, update the `cellColor` indexing to address the new 16x15 buffer instead of the old hardcoded 10x10 layout (depends on T007) — *(stride fixed from hardcoded `10` to `GRID_X`; still only extracts an 8x8 corner for now — full resize is T015)*

**Checkpoint**: Physics core is ported (float-only) and tuned (16x15, fewer pressure iterations) — ready for US2.

---

## Phase 3: User Story 1 - Quick Visual Prototype in the Browser (Priority: P1) 🎯 MVP

**Goal**: A browser-based fluid prototype, resized to the final 16x15 display resolution, that
lets a developer change simulated gravity direction and see the fluid visibly respond.

**Independent Test**: Open `final_project/flip_sim_js/flip_sim.html` with nothing else built
(directly or via the `flip-sim-js-server` container); change the gravity-direction control and
confirm the fluid visibly redirects within a few frames.

### Implementation for User Story 1

- [x] T010 [P] [US1] In `final_project/flip_sim_js/flip_sim.html`, resize the `.grid` CSS (`grid-template-columns`/`grid-template-rows`) from `repeat(8, 50px)` to 16 rows x 15 columns
- [x] T011 [P] [US1] In `final_project/flip_sim_js/flip.js`, update the simulation grid resolution constants to 16x15 so the prototype's internal grid matches the new display size — *(`flip.js` turned out to be fully resolution-agnostic — `fNumX`/`fNumY` are derived from constructor args, no grid-size constants live in this file — so no change was needed here; the actual resize is entirely in `script.js`, see T012)*
- [x] T012 [US1] In `final_project/flip_sim_js/script.js`, update the grid-construction/render loop to build and update a 16x15 cell grid instead of 8x8, reading from the resized `flip.js` state (depends on T010, T011) — *(added `DISPLAY_ROWS`/`DISPLAY_COLS` constants; `setupScene()` now derives `tankWidth`/`h` so the solver's `fNumX`/`fNumY` come out to 15x16 1:1 with the display — verified empirically via `node -e` that the floor-division produces exactly 15/16, not an off-by-one; replaced the old `getMiddle64Colors` 8x8-crop-from-10x10 with a full, uncropped `getGridColors` mapping over the entire grid, matching the "no downsampling" principle used on the C side*
- [ ] T013 [US1] Manually validate per `quickstart.md` Stage 1 scenario: open `final_project/flip_sim_js/flip_sim.html`, confirm the 16x15 grid animates, and confirm the gravity-direction control visibly redirects flow within ~1 second (spec SC-001) (depends on T012) — *automated checks done (JS syntax valid via `node --check`; `nginx` container rebuilt and serves the updated files with HTTP 200; grid math verified numerically); actual visual confirmation in a browser still needs your eyes — no headless-browser tooling is installed in this environment*

**Checkpoint**: User Story 1 is fully functional and independently testable — this is the MVP slice.

---

## Phase 4: User Story 2 - Desktop Simulator for Pre-Hardware Validation (Priority: P2)

**Goal**: The Windows simulator runs the ported/tuned physics core unchanged in logic, rendering
at the same 16x15 resolution as the final hardware.

**Independent Test**: Build (natively or via `flip-sim-c-builder`) and run `final_project/flip_sim_c`
alone; confirm it reproduces fluid motion using the reused (not rewritten) physics source, at
16x15 resolution.

**Depends on**: Phase 2 (Foundational) must be complete.

### Implementation for User Story 2

- [x] T014 [US2] In `final_project/flip_sim_c/util.h`, change `#define SIZE 8` to reflect the new 16-row x 15-column display, and update the window-sizing math in `final_project/flip_sim_c/main.c` (`SIZE * CELL_SIZE + 200` usages) for the non-square 16x15 grid — *(`SIZE` removed entirely; `grid` is now declared `int grid[GRID_Y][GRID_X]` reusing `flip_fluid.h`'s constants via the existing `scene.h` include chain, rather than introducing a second pair of magic numbers; `main.c`'s window-creation call now uses `GRID_X * CELL_SIZE + 200` for width and `GRID_Y * CELL_SIZE + 200` for height)*
- [x] T015 [US2] In `final_project/flip_sim_c/util.c`'s `DrawGrid` and `UpdateGridFromFluid`, remove the old "extract inner 8x8 from a 10x10" cropping logic — the solver grid is now 1:1 with the 16x15 display, so every solver cell maps directly to one displayed cell (depends on T009, T014) — *(`DrawGrid`'s loop bounds changed to `GRID_Y`/`GRID_X`; `UpdateGridFromFluid` rewritten from scratch — dropped the 64-element crop array and the 90°-rotation hack, replaced with a direct per-cell map: display row `i` reads solver row `GRID_Y-1-i` so "up" in the tank renders at the top of the screen, matching the same orientation convention used in the JS rewrite (T012) and in the solver's own `update_cell_colors_from_types`; also updated `InitUI`'s control y-offsets from `SIZE * CELL_SIZE` to `GRID_Y * CELL_SIZE` since the grid is taller now)*
- [x] T016 [US2] Rebuild `final_project/flip_sim_c` (e.g. `MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd):/src" flip-sim-c-builder CC=x86_64-w64-mingw32-gcc`, or native `make`) and confirm it compiles with no new warnings and `main.exe` runs at the new 16x15 resolution (depends on T015) — *rebuilt via Docker: compiled with zero warnings/errors, `file main.exe` confirms `PE32+ executable for MS Windows 5.02 (GUI), x86-64`*
- [x] T017 [US2] Manually validate per `quickstart.md` Stage 2 scenario: run unpaused for several minutes, move the gravity-angle trackbar and confirm consistent flow-direction response (spec SC-002, SC-003), and perform the host-side smoke check (temporarily log `cellColor` and confirm no NaN/inf values appear) (depends on T016) — *the NaN/Inf smoke check is fully automated and passing: a temporary headless harness (`smoke_test.c`, compiled via the mingw Docker image and run directly since this host is Windows, then deleted — not part of the tracked source) called `setupScene`/`simulateFlipFluid` directly for 3000 steps while sweeping the gravity angle -90°..90° exactly as the trackbar would, confirming 252 particles, `fNumX=15`/`fNumY=16`, and zero NaN/Inf across all `cellColor` and `particlePos` values — **PASS**. The GUI-interaction half (visually watching `main.exe`'s window, dragging the actual trackbar, eyeballing flow direction for several minutes) requires a human at the keyboard — I can't drive a Win32 GUI from here; please run `final_project/flip_sim_c/main.exe` yourself to confirm SC-002/SC-003 visually*

**Checkpoint**: User Stories 1 AND 2 both work independently; the physics core is validated on a host PC, ready for the (separately-initiated) Stage 3 firmware phase to copy.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency checks across the two in-scope stages.

- [x] T018 [P] Update `final_project/flip_sim_c/README.md` with the new 16x15 resolution, the Docker build command, and a note describing the physics/display module boundary (`contracts/physics-core.md`), so the Stage 3 phase has an accurate starting point when it begins
- [ ] T019 Run the `quickstart.md` Stage 1 and Stage 2 validation passes back-to-back and record the result (depends on T013, T017) — *blocked on T013's outstanding visual confirmation (no headless-browser/GUI-automation tooling available in this environment); everything automatable (build, compile, headless NaN/Inf smoke test, grid-size math) is done and passing — see T013/T017 notes*

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately (T001–T004 already complete)
- **Foundational (Phase 2)**: No dependency on Setup completion, but sequenced after it for
  clarity — **blocks US2 only**
- **User Story 1 (Phase 3)**: Depends only on Setup (T002/T004) — can run **in parallel with Phase 2**
- **User Story 2 (Phase 4)**: Depends on Foundational (Phase 2) completion
- **Polish (Phase 5)**: Depends on both user stories being complete

### Within Phase 2 (Foundational)

T005 → T006 → T008 are sequential edits to the same files (`flip_fluid.c`, `scene.c`); T007
depends on T006's grid resize; T009 depends on T007. This phase is inherently sequential — it is
a small, tightly-coupled set of edits to a handful of existing files, not a set of independent
modules.

### Parallel Opportunities

- T001–T004 (Phase 1) can all run in parallel
- Phase 3 (User Story 1) can run in parallel with Phase 2 (Foundational) — different
  files/languages, no shared dependency
- T010 and T011 (Phase 3) can run in parallel

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T004) — **done**
2. Complete Phase 3: User Story 1 (T010–T013)
3. **STOP and VALIDATE**: open the browser prototype, confirm SC-001
4. This is demoable on its own — no C toolchain required

### Incremental Delivery

1. Setup (Phase 1, done) → Foundational (Phase 2) can start right after; US1 (Phase 3) can run in
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
  (e.g., T005 vs. T006 vs. T008), so a mistake in one tuning decision (e.g., iteration count) can
  be isolated and reverted without undoing the others
- The Docker setup (T003/T004) is purely a reproducible build/serve environment around the
  existing, unmodified UI code (Win32 sliders/trackbar/buttons in `main.c`/`util.c`); it must not
  be used as a reason to refactor that code to "fit" a container

---

## Phase 6: Round 2 — Post-Implementation Visual QA Fixes

**Purpose**: Fix four issues the user found running Stages 1–2 after Phase 1–5 completed
(research.md Decisions 7–10). All changes are parameter/rendering-only — no FLIP/PIC formula is
rewritten, consistent with the same constraint Phase 2 operated under.

### Phase 6a: User Story 1 (Browser prototype) — Round 2

- [x] T020 [P] [US1] In `final_project/flip_sim_js/script.js`'s `setupScene()`, pad the solver
  grid one cell beyond the display on the walled sides: derive `resX`/`resY`/`tankWidth`/`h` from
  `(DISPLAY_COLS + 2)` / `(DISPLAY_ROWS + 1)` instead of `DISPLAY_COLS` / `DISPLAY_ROWS` directly,
  so `FlipFluid`'s computed `fNumX`/`fNumY` come out one cell larger than the display on
  left/right/bottom (research.md Decision 7) — *added module-level `PAD_FNUM_X`/`PAD_FNUM_Y`
  constants; verified via `node -e` that the padded floor-division produces exactly 17/17*
- [x] T021 [US1] In `final_project/flip_sim_js/script.js`'s `getGridColors()` and `draw()`, update
  the index mapping to read from the padded `fNumX`/`fNumY` grid with a `+1` column offset and a
  flip-plus-offset row mapping (solver's true open top is now at row `fNumY - 1`, one row higher
  than before), writing into the still-exactly-`DISPLAY_ROWS x DISPLAY_COLS` display array; update
  the `cellColor.length === expectedCells` check in `draw()` to the new padded cell count (depends
  on T020) — *verified via a temporary headless harness (`smoke_test.mjs`, run directly with Node
  since `flip.js`'s `FlipFluid` class has no DOM dependency, then deleted) over 3000 steps with a
  full gravity-angle sweep: zero NaN/Inf, and the cropped left/right/bottom display edges now light
  up (27591/29378/39325 hits respectively) while the top stays at 0 hits — confirming the fix works
  and that the top-row sparsity is the documented gravity-direction-range behavior, not a bug*
- [x] T022 [P] [US1] In `final_project/flip_sim_js/script.js`, lower `scene.numPressureIters` from
  `100` to `20` to match Stage 2's value (research.md Decision 8) — independent of T020/T021
- [x] T023 [P] [US1] In `final_project/flip_sim_js/flip_sim.html`, add row index labels (0–15)
  to the left of `.grid` and column index labels (0–14) above it (research.md Decision 9) — plain
  markup/CSS, no JS logic change — *restructured `.grid` into a single `.labeled-grid` CSS grid
  (`30px` label column/row + the original `repeat(15, 50px)`/`repeat(16, 50px)` data cells) so
  labels stay pixel-aligned with the data cells; `script.js`'s grid-creation loop now also creates
  the label cells (`.label` class, untouched by `updateGridColors()`, which still only selects
  `.cell` elements)*
- [x] T024 [P] [US1] In `final_project/flip_sim_js/flip_sim.html` and `script.js`, add visible
  "Start"/"Pause" `<button>` elements wired to toggle `scene.paused`, as the primary control
  (research.md Decision 10); the existing `p` keyboard shortcut may remain as a bonus — *wired via
  `addEventListener` in `script.js` (not inline `onclick=` HTML attributes, since `scene` is
  module-scoped, not global — matching how the existing `gravitySlider` listener already works)*
- [ ] T025 [US1] Manually validate per `quickstart.md` Stage 1 "Round 2 checks": Start/Pause
  buttons work, row/column labels are visible, fluid eventually reaches the left/right/bottom edge
  cells over varied gravity angles, and the fluid's "weight" feels comparable to Stage 2 (depends
  on T021, T022, T023, T024) — *automated checks done (syntax valid, headless NaN/Inf + edge-hit
  test passing, `nginx` container rebuilt and serving the new files with HTTP 200); actual visual
  confirmation in a browser still needs your eyes*

### Phase 6b: User Story 2 (Windows simulator) — Round 2

**Depends on**: none of Phase 6a (different files/language) — can run in parallel with it.

- [x] T026 [US2] In `final_project/flip_sim_c/flip_utils.c`'s `setupScene()`, pad the solver grid
  the same way as T020: set `f->fNumX = GRID_X + 2` and `f->fNumY = GRID_Y + 1` (instead of
  `GRID_X`/`GRID_Y` directly), and recompute `resX`/`resY`/`tankWidth`/`tankHeight`/`h` from those
  padded values (same formulas as the current code, new constants) (research.md Decision 7) —
  *added `PAD_FNUM_X`/`PAD_FNUM_Y` to `flip_fluid.h` (shared with flip_fluid.c's T027 fix) instead
  of inlining `GRID_X+2`/`GRID_Y+1` twice*
- [x] T027 [US2] In `final_project/flip_sim_c/flip_fluid.c`'s `update_cell_colors_from_types`,
  change the read offset so display column `dispX` reads solver column `dispX + 1` and display row
  `dispY` reads solver row `dispY + 1`, bounds-checked against the now-larger `f->fNumX`/`f->fNumY`
  — this keeps writing into the unchanged `GRID_X * GRID_Y`-sized `cellColor` output buffer, so the
  contract in `contracts/physics-core.md` is unaffected (depends on T026) — *verified via a
  temporary headless harness (`smoke_test.c`, compiled via the mingw Docker image, run directly,
  then deleted) over 3000 steps with a full gravity sweep: 323 particles, zero NaN/Inf, and the
  cropped left/right edges now light up (27109/28981 hits); confirmed the same physical
  top-stays-sparse behavior already found and documented for the JS side*
- [x] T028 [US2] Verify `final_project/flip_sim_c/util.c`'s `UpdateGridFromFluid` needs no change —
  it already consumes the post-crop, display-sized (`GRID_X * GRID_Y`) `cellColor` buffer, which is
  unaffected by the solver's internal padding (depends on T027) — *confirmed; updated its
  now-stale comment (it previously said the solver "is sized to GRID_X x GRID_Y," no longer true)
  to describe the crop happening in flip_fluid.c instead*
- [x] T029 [P] [US2] In `final_project/flip_sim_c/util.c`'s `DrawGrid`, add a cell-separator
  border (e.g. `FrameRect`) around each cell in addition to the existing fill (research.md
  Decision 9) — *switched each cell's draw call from `FillRect` to `Rectangle` with a dark-gray
  pen selected, which fills and outlines in one call*
- [x] T030 [P] [US2] In `final_project/flip_sim_c/util.h`, reduce `CELL_SIZE` (research.md
  Decision 9) — purely the on-screen pixel size; `main.c`'s window-sizing math
  (`GRID_X * CELL_SIZE + 200` / `GRID_Y * CELL_SIZE + 200`, from T014) shrinks automatically since
  it already reads `CELL_SIZE`; do NOT touch `tankWidth`/`tankHeight`/`h`/`r`/`resX`/`resY` —
  *50px → 30px; also added `LABEL_MARGIN`/`GRID_LEFT`/`GRID_TOP` (30px) for T031's labels, and
  updated `main.c`'s window-creation call and `util.c`'s `InitUI` control y-offsets to add
  `GRID_TOP`/account for the shifted grid origin*
- [x] T031 [US2] In `final_project/flip_sim_c/util.c` (`DrawGrid` or a new label-drawing function
  called from `main.c`'s `WM_PAINT` handler), draw row indices (0–15) along the left margin and
  column indices (0–14) along the top margin using `TextOut`, reserving extra margin space for them
  (research.md Decision 9) (depends on T030 for final cell-origin offsets) — *added a static
  `DrawGridLabels()` helper in `util.c`, called from `DrawGrid()`, using `DrawText`/`wsprintf`
  rather than raw `TextOut` for centered alignment within each margin cell*
- [x] T032 [US2] Rebuild `final_project/flip_sim_c` (Docker or native `make`) and confirm it
  compiles with no new warnings (depends on T027, T029, T030, T031) — *rebuilt via Docker: zero
  warnings/errors, `file main.exe` confirms `PE32+ executable for MS Windows 5.02 (GUI), x86-64`*
- [x] T033 [US2] Manually validate per `quickstart.md` Stage 2 "Round 2 checks": gridlines and
  row/column labels visible, window noticeably smaller, fluid eventually reaches the
  left/right/bottom edge cells over varied gravity angles; re-run the headless NaN/Inf smoke-test
  pattern from T017 against the now-padded grid to confirm no instability was introduced (depends
  on T032) — *re-ran the headless smoke-test pattern: PASS, no NaN/Inf over 3000 steps with the
  full rendering change set applied; the GUI-visual half (gridlines/labels/window size, by eye)
  still needs you to run `main.exe` — I can't drive a Win32 GUI from here*

### Phase 6c: Polish — Round 2

- [x] T034 [P] Update both `README.md`s (`final_project/flip_sim_c/`, `final_project/flip_sim_js/`)
  with a short note that the solver's internal grid is now padded beyond the 16x15 display on
  walled sides (research.md Decision 7), so future readers aren't confused about `GRID_X`/`GRID_Y`
  still meaning the 15x16 *display* size
- [ ] T035 Run the `quickstart.md` Stage 1 and Stage 2 "Round 2 checks" back-to-back and record the
  result (depends on T025, T033) — *blocked on T025/T033's outstanding visual confirmation; all
  automatable checks (build, compile, headless NaN/Inf + edge-reachability smoke tests for both
  stages) are done and passing*

**Checkpoint**: All four Round 2 issues fixed and validated in both stages; ready to return to the
(separately-initiated) Stage 3 phase with an accurate, polished pre-hardware reference.

---

## Phase 7: Round 3 — Match Stage 2's Playback Speed to Real Water

**Purpose**: Fix the root cause of the two stages still "feeling" like different fluids after
Round 2: Stage 2's physics clock didn't match its own tick rate, so it played back in slow motion
(research.md Decision 11). STM32L4-friendly by construction — the fix is a constant change, not
added computation (same 50Hz tick rate as before).

- [x] T036 [US2] In `final_project/flip_sim_c/scene.c`'s `scene_default()`, change `s.dt` from
  `1.0f / 120.0f` to `0.02f` (matching `main.c`'s existing `TIMER_INTERVAL = 20` ms `WM_TIMER`
  rate — `scene.c` can't include `main.c`, so this is a hardcoded value with a comment pointing
  back at `TIMER_INTERVAL` as the source of truth to keep in sync) (research.md Decision 11)
- [x] T037 [P] Polish: row/column index labels in both stages now display 1-based numbers
  (1–15/1–16) instead of 0-based, per direct user feedback mid-round — `final_project/flip_sim_c/
  util.c`'s `DrawGridLabels` and `final_project/flip_sim_js/script.js`'s label-creation loops
- [x] T038 [US2] Rebuild `final_project/flip_sim_c` (Docker) and confirm it compiles with no new
  warnings (depends on T036, T037)
- [x] T039 [US2] Re-verify via the headless smoke-test pattern (T017/T033) that `dt = 0.02f` with
  the existing `numPressureIters = 20` produces no NaN/Inf and no escaped particles over 3000
  steps with a full gravity sweep (depends on T036) — *re-ran a second time against the actual
  `scene_default()` (not just an override) after T036 landed: PASS, 323 particles, zero bad
  frames; `main.exe` rebuilt clean afterward (T038), `git status` confirms no stray source diffs*
- [ ] T040 [US2] Manually validate per `quickstart.md`'s Round 3 check: run `main.exe`, confirm
  fluid motion now plays at real-world speed (not slow motion) and feels comparable to Stage 1
  (depends on T038, T039) — *all automatable checks pass; this needs your eyes on the actual
  window, same as T013/T025/T033 before it*

**Checkpoint**: Both stages now share matching iteration count (Round 2) and matching real-time
playback speed (Round 3) — the two remaining "feel" differences the user reported are resolved.

---

## Phase 8: Round 4 — Anti-Flicker Hysteresis Fix

**Purpose**: Stop surface LEDs from blinking when the fluid is at rest (research.md Decision 12)
by switching the display threshold from the binary `cellType` flag to a hysteresis check on the
already-computed `particleDensity`. Solver functions (`integrate_particles`,
`push_particles_apart`, `transfer_velocities`, `solve_incompressibility`) are NOT touched — this
is entirely inside the existing solver-state-to-display-buffer conversion step.

### Phase 8a: User Story 1 (Browser prototype) — Round 4

- [x] T041 [P] [US1] In `final_project/flip_sim_js/flip.js`, add `LED_ON_THRESHOLD`/
  `LED_OFF_THRESHOLD` constants (0.5/0.2, fractions of `particleRestDensity`) and a persisted
  `this.ledState = new Float32Array(this.fNumCells)` (zero-initialized) in the `FlipFluid`
  constructor
- [x] T042 [US1] In `final_project/flip_sim_js/flip.js`'s `updateCellColors()`, replace the
  `cellType[i] == FLUID_CELL` check with: normalize `particleDensity[i]` against
  `particleRestDensity` (guard divide-by-zero before rest density is computed), turn `ledState[i]`
  on above `LED_ON_THRESHOLD`, off below `LED_OFF_THRESHOLD`, otherwise leave it unchanged; write
  `cellColor[i] = ledState[i]` (same buffer/size as before — `script.js`'s consumers need no
  change) (depends on T041)
- [x] T043 [US1] Verify headlessly (extend the `smoke_test.mjs` pattern from Round 1/2, temporary,
  delete after use): run until the fluid settles with gravity held constant, then count how many
  display cells change state per subsequent frame — confirm it drops to ~0 at rest (vs. the
  pre-fix binary version) and confirm no NaN/Inf regression over a full gravity sweep (depends on
  T042) — *measured 193→134 flips over 600 post-settle frames (~30% reduction), zero NaN/Inf.
  Confirmed it does NOT drop to ~0: swept hysteresis gap width (up to ON=0.9/OFF=0.05), added an
  EMA smoothing pass, and added up to a 20-frame minimum dwell time — none reduced the count
  further, which rules out noise/bouncing as the cause. The residual is genuine sustained surface
  settling motion (low damping, `flipRatio=0.9`, by design), not a remaining defect — see
  research.md Decision 12's "Empirical finding" for the full reasoning. Reported this honestly
  rather than over-claim a full fix.*

### Phase 8b: User Story 2 (Windows simulator) — Round 4

**Depends on**: none of Phase 8a (different files/language) — can run in parallel with it.

- [x] T044 [US2] In `final_project/flip_sim_c/flip_fluid.h`, add `#define LED_ON_THRESHOLD 0.5f`
  and `#define LED_OFF_THRESHOLD 0.2f` (fractions of `g_particleRestDensity`)
- [x] T045 [US2] In `final_project/flip_sim_c/flip_fluid.c`, add a persisted
  `static float g_ledState[GRID_X * GRID_Y]` (zero-initialized) and rewrite
  `update_cell_colors_from_types` to read `g_particleDensity[src]` instead of checking
  `g_cellType[src] == FLUID_CELL`, normalize against `g_particleRestDensity` (guard
  divide-by-zero), apply the hysteresis update to `g_ledState[dst]` instead of writing `outColors`
  directly, then `outColors[dst] = g_ledState[dst]` — keep the existing crop/offset loop structure
  (research.md Decision 7) unchanged, just swap what's read inside it (depends on T044) —
  *confirmed `update_particle_density(f)` already runs before this function in
  `simulateFlipFluid` (step 4, before step 5's color update), so `g_particleDensity`/
  `g_particleRestDensity` are always current when read here*
- [x] T046 [US2] Rebuild `final_project/flip_sim_c` (Docker) and confirm it compiles with no new
  warnings (depends on T045) — *zero warnings/errors, valid PE32+ binary*
- [x] T047 [US2] Verify headlessly (extend the `smoke_test.c` pattern from prior rounds, temporary,
  delete after use): run until settled with gravity held constant, count display-cell state
  changes per frame at rest (confirm it drops to ~0 vs. the pre-fix version), and confirm no
  NaN/Inf over a full gravity sweep (depends on T046) — *323 particles, zero NaN/Inf; 120 flips
  over 600 post-settle frames — same order of magnitude as JS's 134 (T043), same underlying cause
  (genuine sustained surface settling, not noise — see research.md Decision 12); does not drop to
  ~0, and per T043's investigation, no further display-layer tuning changes that — documented as
  the honest, final result rather than re-claiming the original "~0" target*

### Phase 8c: Polish — Round 4

- [ ] T048 Manually validate per `quickstart.md`'s Round 4 check in both apps: hold the gravity
  slider/trackbar still for 10+ seconds, confirm the boundary LEDs hold a stable pattern with no
  per-frame blinking, then confirm the display still responds promptly once the control is moved
  again (depends on T043, T047) — *automated measurement shows real, sustained surface motion
  still continues at rest (research.md Decision 12's empirical finding) — expect a slow, low-level
  drift along the boundary, not a fix to perfect stillness; this needs your eyes to judge whether
  that residual motion now reads as "settling water" rather than "flicker"*
- [x] T049 [P] Update both `README.md`s with a short note that the display layer now uses a
  hysteresis threshold on `particleDensity` instead of the raw binary `cellType`, to prevent
  surface flicker at rest (research.md Decision 12)

**Checkpoint**: Both stages hold a stable display at rest while remaining responsive to real
gravity changes — the flicker the user reported is resolved, with no solver-formula changes.

---

## Phase 9: Round 5 — Continuous Brightness Display (supersedes Round 4)

**Purpose**: Replace the binary on/off display (and Round 4's hysteresis patch over it) with
continuous per-cell brightness rendered directly from `particleDensity`/`particleRestDensity`
(research.md Decision 13), researched against real fluid-pendant projects. Strictly *less*
computation than Round 4 (no persisted state, no branching) — solver functions untouched.

### Phase 9a: User Story 1 (Browser prototype) — Round 5

- [x] T050 [P] [US1] In `final_project/flip_sim_js/flip.js`, remove `LED_ON_THRESHOLD`/
  `LED_OFF_THRESHOLD` and `this.ledState` (superseded by Decision 13)
- [x] T051 [US1] In `final_project/flip_sim_js/flip.js`'s `updateCellColors()`, replace the
  hysteresis logic with `this.cellColor[i] = clamp(normalized, 0, 1)`, where `normalized =
  particleRestDensity > 0 ? particleDensity[i] / particleRestDensity : 0` (reuse the existing
  module-level `clamp()` helper) (depends on T050) — `script.js`'s `getGridColors()` already
  renders `cellColor` as `rgb(0, g*255, 0)`, i.e. already brightness-correct; no change needed there
- [x] T052 [US1] Verify headlessly (temporary harness, delete after use): confirm no NaN/Inf over a
  full gravity sweep, and confirm `cellColor` now spans a continuous range (print min/max or a
  histogram after settling) rather than only ever being exactly 0 or 1 (depends on T051) — *0 bad
  frames; min=0.0000 max=1.0000, 152/240 cells with intermediate (non-0/1) brightness after settling*

### Phase 9b: User Story 2 (Windows simulator) — Round 5

**Depends on**: none of Phase 9a (different files/language) — can run in parallel with it.

- [x] T053 [US2] In `final_project/flip_sim_c/flip_fluid.h`, remove `LED_ON_THRESHOLD`/
  `LED_OFF_THRESHOLD` (superseded by Decision 13)
- [x] T054 [US2] In `final_project/flip_sim_c/flip_fluid.c`, remove `g_ledState` and rewrite
  `update_cell_colors_from_types` to write `outColors[dst] = ff_clamp(normalized, 0.0f, 1.0f)`
  directly (reuse the existing `ff_clamp` helper already used elsewhere in this file) (depends on
  T053)
- [x] T055 [US2] In `final_project/flip_sim_c/util.c`, change `grid[GRID_Y][GRID_X]`'s semantics
  from binary 0/1 to a 0-255 brightness level: `UpdateGridFromFluid` scales `(int)(g * 255.0f)`
  instead of thresholding `g > 0.01f`, and `DrawGrid`'s brush color becomes `RGB(0, grid[i][j], 0)`
  instead of the `grid[i][j] ? RGB(0,255,0) : RGB(0,0,0)` ternary (depends on T054)
- [x] T056 [US2] Rebuild `final_project/flip_sim_c` (Docker) and confirm it compiles with no new
  warnings (depends on T055) — *zero warnings/errors, valid PE32+ binary*
- [x] T057 [US2] Verify headlessly (temporary harness, delete after use): confirm no NaN/Inf over a
  full gravity sweep, and confirm `cellColor` now spans a continuous range rather than only ever
  being exactly 0 or 1 (depends on T056) — *0 bad frames; min=0.0000 max=1.0000, 74/240 cells with
  intermediate brightness after settling*

### Phase 9c: Polish — Round 5

- [ ] T058 Manually validate per `quickstart.md`'s Round 5 check in both apps: confirm a visible
  brightness gradient at the fluid surface (dimmer near the edge, brighter toward the interior)
  instead of a hard on/off line (depends on T052, T057) — *needs your eyes; the JS preview's green
  channel scales linearly with brightness (`rgb(0, g*255, 0)`) and the C app's `DrawGrid` does the
  same via `grid[i][j]` 0-255, so the surface should now look like a graded dark-to-bright fade
  rather than a hard black/green line*
- [x] T059 [P] Update both `README.md`s: replace the Round 4 hysteresis note with a Round 5
  continuous-brightness note, crediting the researched reference projects briefly (research.md
  Decision 13 Sources)

**Checkpoint**: Both stages render a realistic continuous brightness gradient instead of a binary
on/off pattern, validated against real fluid-pendant projects, with no increase in per-tick
computation versus Round 4.

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
