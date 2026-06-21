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
