import { FlipFluid } from "./flip.js";

///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Utility Functions /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

export function updateGravity(angle) {
  let radians = (angle * Math.PI) / 180; // Convert degrees to radians
  scene.gravity_x = Math.sin(radians) * 9.81; // X component
  scene.gravity_y = -Math.cos(radians) * 9.81; // Y component (negative to simulate downward gravity)

  document.getElementById("gravityAngle").innerText = `${angle}°`;
}

// Add event listener for gravitySlider in script.js
document.getElementById("gravitySlider").addEventListener("input", function () {
  updateGravity(this.value); // Call updateGravity function when the slider value changes
});

document.addEventListener("keydown", (event) => {
  switch (event.key) {
    case "p":
      scene.paused = !scene.paused;
      break;
    case "m":
      scene.paused = false;
      simulate();
      scene.paused = true;
      break;
  }
});

// Display resolution: 1:1 with the final 16-row x 15-column charlieplexed LED matrix.
const DISPLAY_ROWS = 16;
const DISPLAY_COLS = 15;

// The solver's own grid is padded one cell beyond the display on every walled side
// (left, right, bottom) so those structurally-solid wall cells fall outside the
// visible window instead of being its outermost ring — see setupScene() and
// getGridColors() below, and research.md Decision 7.
const PAD_FNUM_X = DISPLAY_COLS + 2;
const PAD_FNUM_Y = DISPLAY_ROWS + 1;

// Create the 16x15 grid dynamically, plus a row of column-index labels (1-15)
// above it and a column of row-index labels (1-16) to its left (1-based for
// display only; the underlying col/row loop variables stay 0-indexed). Both
// share one CSS grid (`.labeled-grid`) so they line up with the data cells;
// only `.cell` elements (not `.label` elements) are touched by updateGridColors() below.
const gridContainer = document.getElementById("labeledGrid");
gridContainer.appendChild(document.createElement("div")).classList.add("label"); // corner spacer
for (let col = 0; col < DISPLAY_COLS; col++) {
  let colLabel = document.createElement("div");
  colLabel.classList.add("label");
  colLabel.textContent = col + 1;
  gridContainer.appendChild(colLabel);
}
for (let row = 0; row < DISPLAY_ROWS; row++) {
  let rowLabel = document.createElement("div");
  rowLabel.classList.add("label");
  rowLabel.textContent = row + 1;
  gridContainer.appendChild(rowLabel);
  for (let col = 0; col < DISPLAY_COLS; col++) {
    let cell = document.createElement("div");
    cell.classList.add("cell");
    gridContainer.appendChild(cell);
  }
}

// Start/Pause buttons (primary control) — the "p" keyboard shortcut above remains as a bonus.
document.getElementById("startButton").addEventListener("click", () => {
  scene.paused = false;
});
document.getElementById("pauseButton").addEventListener("click", () => {
  scene.paused = true;
});

////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Start Simulator ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

var simHeight = 3.0;

var scene = {
  gravity_x: 0.0,
  gravity_y: -9.81,
  dt: 1.0 / 120.0, // 1/120 = 0.0083 means the simulation runs at 120 FPS (frames per second)
  flipRatio: 0.9, // FLIP conserves vorticity better (more swirly motion), while PIC is more stable.
  numPressureIters: 20, // Number of iterations for the pressure solver (responsible for keeping water incompressible). Matches Stage 2's value so both stages feel comparably "dense" (research.md Decision 8).
  numParticleIters: 2, //Number of iterations for particle-based corrections (to avoid unrealistic compression).
  frameNr: 0,
  overRelaxation: 1.9,
  compensateDrift: true,
  separateParticles: true,
  paused: true,
  showParticles: true,
  showGrid: true,
  fluid: FlipFluid, // Holds the FLIP fluid simulation object (set up later).
};

function setupScene() {
  // The solver's grid is padded one cell beyond the display on every walled side
  // (left, right, bottom — see the wall-setup loop below), so those wall cells
  // fall outside the visible DISPLAY_COLS x DISPLAY_ROWS window instead of
  // coinciding with its outermost ring (which would otherwise make those display
  // cells permanently unable to show fluid, since wall cells never become FLUID).
  // The top has no wall, so it isn't padded. fNumX/fNumY (computed inside
  // FlipFluid's constructor as floor(width/h)+1 and floor(height/h)+1) come out to
  // PAD_FNUM_X/PAD_FNUM_Y, so resX/resY here are one less than those padded
  // targets. Cells stay square (same h both directions) by deriving tankWidth
  // from the same cell size h as tankHeight.
  var resX = PAD_FNUM_X - 1;
  var resY = PAD_FNUM_Y - 1;

  var tankHeight = simHeight;
  var h = tankHeight / resY; // grid cell size
  var tankWidth = resX * h;
  var density = 10.0;

  var relWaterHeight = 0.8;
  var relWaterWidth = 0.8;

  // compute number of particles

  var r = 0.3 * h; // particle radius w.r.t. cell size. not diameter!!!
  var dx = 2.0 * r; // Particle spacing in the x-direction (2 radii)
  var dy = (Math.sqrt(3.0) / 2.0) * dx; // Particle spacing in the y-direction using hexagonal close-packing. equilateral triabgle. 60deg angle. 2r*sin60deg = (sqrt(3)/2)*2r

  var numX = Math.floor((relWaterWidth * tankWidth - 2.0 * h - 2.0 * r) / dx);
  var numY = Math.floor((relWaterHeight * tankHeight - 2.0 * h - 2.0 * r) / dy);
  var maxParticles = numX * numY; //  total number of particles.

  // create fluid
  var f = (scene.fluid = new FlipFluid(
    density,
    tankWidth,
    tankHeight,
    h,
    r,
    maxParticles // used only in the constructor
  ));

  f.numParticles = numX * numY; // used inside the important functions. not used much in the constructor of FlipFluid

  // Loops through numX × numY, placing particles in a hexagonal grid.
  var p = 0;
  for (var i = 0; i < numX; i++) {
    for (var j = 0; j < numY; j++) {
      f.particlePos[p++] = h + r + dx * i + (j % 2 == 0 ? 0.0 : r);
      f.particlePos[p++] = h + r + dy * j;
    }
  }

  // setup grid cells for tank
  var n = f.fNumY;
  for (var i = 0; i < f.fNumX; i++) {
    for (var j = 0; j < f.fNumY; j++) {
      var s = 1.0; // fluid
      if (i == 0 || i == f.fNumX - 1 || j == 0) s = 0.0; // solid
      f.s[i * n + j] = s;
    }
  }
}

function draw() {
  const expectedCells = PAD_FNUM_X * PAD_FNUM_Y;
  if (scene.fluid.cellColor && scene.fluid.cellColor.length === expectedCells) {
    updateGridColors(scene.fluid.cellColor);
  } else {
    console.error("Invalid cell color data");
  }
}

// Crop the padded fNumX x fNumY solver grid down to the DISPLAY_ROWS x
// DISPLAY_COLS display: display column c reads solver column (c + 1), skipping
// the solver's left/right wall columns; the solver's open top (no wall) is at
// solver row (PAD_FNUM_Y - 1), so display row r (0 = top of screen) reads solver
// row (PAD_FNUM_Y - 1 - r), skipping only the solver's bottom wall row.
// FlipFluid stores cells as cellColor[x * fNumY + y].
function getGridColors(cellColor) {
  let colors = [];
  for (let row = 0; row < DISPLAY_ROWS; row++) {
    for (let col = 0; col < DISPLAY_COLS; col++) {
      let simX = col + 1;
      let simY = PAD_FNUM_Y - 1 - row;
      let cellIndex = simX * PAD_FNUM_Y + simY;
      let g = cellColor[cellIndex] || 0;
      colors.push(`rgb(0, ${g * 255}, 0)`);
    }
  }
  return colors;
}

// Function to update the grid colors
function updateGridColors(cellColor) {
  const colors = getGridColors(cellColor);
  document.querySelectorAll(".cell").forEach((cell, index) => {
    cell.style.backgroundColor = colors[index];
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
function simulate() {
  if (!scene.paused)
    scene.fluid.simulate(
      scene.dt,
      scene.gravity_x,
      scene.gravity_y,
      scene.flipRatio,
      scene.numPressureIters,
      scene.numParticleIters,
      scene.overRelaxation,
      scene.compensateDrift,
      scene.separateParticles,
      scene.colorFieldNr
    );
  scene.frameNr++;
}

function update() {
  simulate();
  draw();
}

// Run the simulation at 60 FPS
setupScene();
setInterval(update, 1000 / 120);
