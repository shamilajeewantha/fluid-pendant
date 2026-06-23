# Contract: Physics Core Module

**Source files**: `flip_fluid.c` / `flip_fluid.h`, `flip_utils.c` / `flip_utils.h`,
`scene.c` / `scene.h` (ported verbatim — see research.md Decisions 1–3 for the only permitted
changes: iteration-count tuning, `double`→`float`, and grid/particle resize constants).

## Contract

- MUST compile and run with **no STM32 HAL includes, no GPIO/SPI/timer register references, and no
  `<windows.h>` dependency** — this is what makes it buildable identically on Stage 2 (Windows)
  and Stage 3 (firmware). (constitution Principle III; spec FR-008, SC-006)
- MUST expose its public surface only through `flip_fluid.h` / `flip_utils.h` / `scene.h`; any
  internal helper (e.g., `push_particles_apart`, `solve_incompressibility`) stays `static`.
- MUST use `float` exclusively for all simulation math — no `double` anywhere in this module
  (research.md Decision 2).
- Inputs: a `Scene*` (gravity vector, dt, solver iteration counts, flags) and time step `dt`.
- Outputs: the `FlipFluid.cellColor` buffer, sized to match the configured `fNumX` × `fNumY`
  (16 × 15 for this feature) — this is the sole hand-off point to the display layer (consumed as
  `DisplayFrame` per data-model.md).
- MUST NOT call into the display driver or sensor driver modules, directly or indirectly.

## Consumers

- Stage 2's Win32 UI layer (`util.c`/`main.c`) reads `cellColor` to paint the simulator window.
- Stage 3's `main.c` glue reads `cellColor`, builds a `DisplayFrame`, and hands it to the display
  driver module — `main.c` is glue only and is not part of this contract.

## Verification

- Stage 2 (Windows build) compiling and running this module unmodified in logic is itself the
  primary verification that the module stays host-portable, ahead of any firmware build.

## Round 9 status (2026-06-23, research.md Decisions 18–22) — reconciled against as-built code

This module is now ported onto `axelor` (`Core/Src/flip_fluid.c`, `Core/Src/flip_utils.c`,
`Core/Src/scene.c` + matching `Core/Inc/*.h`) — confirmed by inspection that the source already
satisfied the "no HAL, no `<windows.h>`" requirement without modification to its simulation logic.
One adaptation was required, confined entirely to allocation strategy, not simulation logic:
`malloc`/`calloc`/`free` replaced with fixed-size `static` arrays (`FF_GRID_CELLS`=289,
`FF_MAX_PARTICLES`=323, `FF_HASH_CELLS`=625 — exact values for this tank's one fixed configuration,
not arbitrary margins), since this firmware has exactly one tank/grid configuration, never
runtime-variable (Decision 19). As a direct consequence, `<stdlib.h>` is no longer included by
either `.c` file at all (no heap, no allocation calls of any kind) — `flip_fluid.c` now includes
only `<string.h>`/`<math.h>`; `flip_utils.c` only `<math.h>`/`<stdio.h>`.

**Correction to the original Decision 20**: this status previously also claimed a stray
non-`floorf` `floor` call was fixed at `flip_utils.c:39`. That finding was a false positive from an
imprecise grep that matched the word "floor" inside a comment, not an actual function call —
re-verified directly against the as-ported files with `grep -n "[^f]floor("`: zero matches in
either file. No `floor`→`floorf` change was made because none was needed; every real call already
used `floorf`. See research.md Decision 20's corrected entry and tasks.md T091.

The simulation tick runs on a 20ms main-loop period matching `Scene.dt` (Decision 21), with the
accelerometer-derived `gravity_x`/`gravity_y` magnitude-clamped in `main.c` *before* this module
ever sees them (Decision 22). `main.c`/`util.{c,h}` (Win32 GUI/trackbar glue) were confirmed
Win32-only and excluded from the port entirely; the `flip_sim_c/` source directory the user copied
in remains in the tree as a pre-conversion reference copy, explicitly excluded from the CubeIDE
build (`.cproject`'s `excluding="Src/flip_sim_c"`) rather than deleted.

**One adaptation found only during implementation, not anticipated by Decisions 18–20**:
`flip_utils.c`'s `setupScene()` calls `printf()` once (unchanged from the canonical source) — but
nothing in `axelor` had ever exercised `printf` before (every existing print uses
`sprintf`+`HAL_UART_Transmit` directly), so the weak, undefined `__io_putchar` `syscalls.c` falls
through to would have caused a HardFault the first time this module's setup function ran. Fixed in
`main.c` (hardware-aware glue, not this module) by adding a real `__io_putchar` that routes through
`HAL_UART_Transmit(&huart1, ...)` — this module's own source needed no change, preserving its
"unmodified, verbatim port" status.

Build-verified (headless STM32CubeIDE build): 0 errors, 0 warnings (after fixing 4 new `-Waddress`
warnings — dead null-checks against the new static arrays, which can never be `NULL`). Release
config: Flash (text+data) ≈ 9.4% of 256 KB, RAM (data+bss) ≈ 50.0% of 64 KB — both within the
constitution's 80% gate (tasks.md T093).
