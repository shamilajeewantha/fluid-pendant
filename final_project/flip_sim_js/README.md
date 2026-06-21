# FLIP Fluid — Browser Prototype (Stage 1)

Quick-iteration FLIP/PIC fluid prototype, used to try out physics/visual changes fast before
porting them to the C simulator (Stage 2) and eventually the STM32L431CC firmware (Stage 3).

Display resolution: 16 rows x 15 columns (240 cells), 1:1 with the final charlieplexed LED matrix
— see `DISPLAY_ROWS`/`DISPLAY_COLS` in `script.js`. The solver's own internal grid
(`PAD_FNUM_X`/`PAD_FNUM_Y`, also in `script.js`) is one cell *larger* than that on every walled
side (left/right/bottom) — those wall cells would otherwise coincide with, and permanently blank
out, the display's own outermost ring of LEDs. `getGridColors()` crops the padding away when
mapping `cellColor` onto the `.cell` elements.

`FlipFluid.updateCellColors()` (in `flip.js`) lights a cell using a **hysteresis threshold on
`particleDensity`** (already computed every tick for the pressure solver's drift compensation)
rather than the raw binary `cellType` flag — `LED_ON_THRESHOLD`/`LED_OFF_THRESHOLD` in `flip.js`.
A binary flag flips fully the instant a jittering particle crosses one exact cell boundary;
thresholding the smoother density value, with a gap between the on/off thresholds, prevents that
sub-pixel jitter from blinking an LED every frame at rest.

## Run directly (no server needed)

Open `flip_sim.html` in a browser. It's loaded as an ES module (`<script type="module">`), which
most browsers allow to run straight from a `file://` path; if your browser blocks module imports
over `file://`, use the Docker option below instead.

## Run via Docker

Serves the static files with `nginx:alpine`:

```sh
docker build -t flip-sim-js-server .
docker run --rm -p 8080:80 flip-sim-js-server
```

- `docker build -t flip-sim-js-server .` reads the `Dockerfile`, which starts from the
  `nginx:alpine` base image and copies this folder's files into nginx's web root — producing an
  image (tagged `flip-sim-js-server`) that just serves these static files, nothing else.
- `docker run --rm -p 8080:80 flip-sim-js-server` starts a container from that image and maps
  container port 80 (where nginx listens) to port 8080 on your machine, so the page is reachable
  at `http://localhost:8080/...`. `--rm` deletes the container when it stops (the image stays, so
  the next `docker run` is instant — no rebuild needed unless the files change).

Then open http://localhost:8080/flip_sim.html.

## Controls

- Start/Pause buttons: primary run control (the `p` key also toggles this as a bonus shortcut).
- Gravity Direction slider: redirects simulated gravity -90°..90°; the fluid should visibly
  respond within about a second.
- Particles / Grid / Compensate Drift / Separate Particles checkboxes and the PIC/FLIP slider are
  solver-tuning toggles (not all wired into the current render loop).
