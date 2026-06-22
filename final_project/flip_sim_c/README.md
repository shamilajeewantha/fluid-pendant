# FLIP Fluid — Windows Simulator (Stage 2)

Reuses the FLIP/PIC physics core unchanged in logic (only parameter-tuned: float-only math,
20 pressure-solver iterations, grid resized to 16 rows x 15 columns) to pre-validate the
simulation on a desktop PC before it runs on the STM32L431CC firmware (Stage 3, separate phase).

Display resolution: 16 rows x 15 columns (240 cells), 1:1 with the final charlieplexed LED matrix
— see `GRID_X`/`GRID_Y` in `flip_fluid.h`. The solver's own internal grid (`PAD_FNUM_X`/
`PAD_FNUM_Y`, also in `flip_fluid.h`) is one cell *larger* than that on every walled side
(left/right/bottom) — those wall cells would otherwise coincide with, and permanently blank out,
the display's own outermost ring of LEDs. `update_cell_colors_from_types()` in `flip_fluid.c`
crops the padding away, so `cellColor` itself is still exactly `GRID_X * GRID_Y` (see Module
boundary below).

`update_cell_colors_from_types()` lights a cell using a **hysteresis threshold on
`g_particleDensity`** (already computed every tick for the pressure solver's drift compensation)
rather than the raw binary `g_cellType` flag — `LED_ON_THRESHOLD`/`LED_OFF_THRESHOLD` in
`flip_fluid.h` (research.md Decision 12, reinstated by Decision 15). A binary flag flips fully the
instant a jittering particle crosses one exact cell boundary; thresholding the smoother density
value, with a gap between the on/off thresholds, prevents that sub-pixel jitter from blinking an
LED every frame at rest. The output is binary, not a brightness gradient — an earlier round (now
reverted) briefly rendered continuous brightness, but the real charlieplexed hardware (see
`stm_projects/sample-charlieplexing`) has no per-LED PWM, so the simulator was brought back to
binary to keep matching what the real device can actually display.

## Build natively (Windows + MinGW)

```sh
make
./main.exe
```

- `make` reads the `Makefile` and invokes `gcc` (your installed MinGW) to compile
  `flip_fluid.c flip_utils.c main.c scene.c util.c` into `main.exe`, linking `comctl32` (the
  Windows common-controls library the trackbar/buttons need) with `-mwindows` (GUI subsystem, no
  console window).
- `./main.exe` runs the compiled simulator directly.

## Build via Docker (no MinGW install required)

Cross-compiles with `mingw-w64` inside a Linux container; the bind mount writes the resulting
Windows PE binary straight onto the host, where it runs natively (Docker is the build environment
only — `main.exe` is not run inside the container).

**Git Bash:**

```sh
docker build -t flip-sim-c-builder .
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd):/src" flip-sim-c-builder CC=x86_64-w64-mingw32-gcc
./main.exe
```

**PowerShell:**

```powershell
docker build -t flip-sim-c-builder .
docker run --rm -v "${PWD}:/src" flip-sim-c-builder CC=x86_64-w64-mingw32-gcc
.\main.exe
```

- `docker build -t flip-sim-c-builder .` reads the `Dockerfile` to build an image containing
  Debian + the `mingw-w64` cross-compiler + `make`, tagged `flip-sim-c-builder`. The image is a
  build environment only — it never runs `main.exe`.
- `docker run --rm -v ...:/src flip-sim-c-builder CC=x86_64-w64-mingw32-gcc`:
  - `-v "<this folder>:/src"` bind-mounts the current folder into the container at `/src`, so the
    container's `make` reads/writes these exact files on your host — no copy-out step needed.
  - The image's `ENTRYPOINT` is `make`, so this line effectively runs `make CC=x86_64-w64-mingw32-gcc`
    inside the container, telling the existing `Makefile` to compile with the MinGW cross-compiler
    instead of a native `gcc`, producing a genuine Windows PE32+ binary even though the compiler
    itself ran on Linux.
  - `--rm` deletes the container (not the image, not the mounted files) once the build finishes.
  - `MSYS_NO_PATHCONV=1` (Git Bash only) stops Git Bash from mangling the `host:container` volume
    path before Docker sees it — without it the bind mount silently mounts an empty directory.
- `./main.exe` / `.\main.exe` then runs the freshly cross-compiled binary, on Windows, outside
  Docker, exactly like the native build above.

## Module boundary

`flip_fluid.c`/`.h`, `flip_utils.c`/`.h`, and `scene.c`/`.h` are the physics core: no
`<windows.h>`, no GPIO/SPI/timer references, `float`-only math, and a single hand-off point — the
`FlipFluid.cellColor` buffer (sized `GRID_X * GRID_Y`). This is what lets the same source compile
unmodified for the Stage 3 firmware later. `main.c`/`util.c`/`util.h` are Win32 UI glue
(window, trackbar, buttons) that reads `cellColor` to paint this simulator's window — they are not
part of the physics core and are not expected to be reused on the firmware. See
`specs/001-multi-stage-fluid-sim/contracts/physics-core.md` for the full contract.
