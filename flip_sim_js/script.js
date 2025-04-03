import { FlipFluid } from "./flip.js";

///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Utility Functions /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

export function updateGravity(angle) {
  let radians = (angle * Math.PI) / 180; // Convert degrees to radians
  scene.gravity.x = Math.sin(radians) * 9.81; // X component
  scene.gravity.y = -Math.cos(radians) * 9.81; // Y component (negative to simulate downward gravity)

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

// Create the 8x8 grid dynamically
const gridContainer = document.getElementById("grid");
for (let i = 0; i < 64; i++) {
  let cell = document.createElement("div");
  cell.classList.add("cell");
  gridContainer.appendChild(cell);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Start Simulator ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

var simHeight = 3.0;

var scene = {
  gravity: { x: 0, y: -9.81 },

  // gravity: -9.81,
  //		gravity : 0.0,
  dt: 1.0 / 120.0, // 1/120 = 0.0083 means the simulation runs at 120 FPS (frames per second)
  flipRatio: 0.9, // FLIP conserves vorticity better (more swirly motion), while PIC is more stable.
  numPressureIters: 100, // Number of iterations for the pressure solver (responsible for keeping water incompressible). Higher values reduce artifacts but increase computation time.
  numParticleIters: 2, //Number of iterations for particle-based corrections (to avoid unrealistic compression).
  frameNr: 0,
  overRelaxation: 1.9,
  compensateDrift: true,
  separateParticles: true,
  obstacleX: 0.0, // Defines an obstacle in the fluid that orange ball
  obstacleY: 0.0, // Defines an obstacle in the fluid that orange ball
  obstacleRadius: 0.15, // Defines an obstacle in the fluid that orange ball
  paused: true,
  showObstacle: true, // Defines an obstacle in the fluid that orange ball
  obstacleVelX: 0.0, // Velocity of the obstacle (not moving initially). orange ball
  obstacleVelY: 0.0, // Velocity of the obstacle (not moving initially). orange ball
  showParticles: true,
  showGrid: true,
  fluid: null, // Holds the FLIP fluid simulation object (set up later).
};

function setupScene() {
  // res = 16 means the tank is divided into 16 grid cells in height.
  var res = 9;

  var tankHeight = 1.0 * simHeight;
  var tankWidth = 1.0 * simHeight;
  var h = tankHeight / res; // grid cell size
  var density = 1000.0;

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
  if (scene.fluid.cellColor && scene.fluid.cellColor.length === 100) {
    updateGridColors(scene.fluid.cellColor);
  } else {
    console.error("Invalid cell color data");
  }
  // console.log(JSON.stringify(scene.fluid.cellColor, null, 2));
  // it logs the color grid like this. 3 for each cell. for 100 cells, it will be 300 values
  //  {
  //   "0": 0.5,
  //   "1": 0.5,
  //   "2": 0.5,
  //   "3": 0.5,
  //   "4": 0.5,
  //   "5": 0.5,
  //   "6": 0.5,
  //  }
}

// Extract middle 64 cells from the 10×10 grid
function getMiddle64Colors(cellColor) {
  let middleColors = [];
  for (let col = 8; col >= 1; col--) {
    for (let row = 1; row <= 8; row++) {
      let cellIndex = row * 10 + col; // Each cell has 3 values (RGB)
      let g = cellColor[cellIndex] || 0;
      middleColors.push(`rgb(0, ${g * 255}, 0)`);
    }
  }
  return middleColors;
}

// Function to update the grid colors
function updateGridColors(cellColor) {
  const colors = getMiddle64Colors(cellColor);
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
      scene.gravity,
      scene.flipRatio,
      scene.numPressureIters,
      scene.numParticleIters,
      scene.overRelaxation,
      scene.compensateDrift,
      scene.separateParticles,
      scene.obstacleX,
      scene.obstacleY,
      scene.obstacleRadius,
      scene.colorFieldNr
    );
  scene.frameNr++;
}

function update() {
  simulate();
  draw();
  requestAnimationFrame(update);
}

setupScene();
update();
