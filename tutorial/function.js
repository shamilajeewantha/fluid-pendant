let x = 10;

function modifyX() {
  x = 20; // This only modifies the local copy
  console.log("Inside modifyX:", x); // Output: 20
}

modifyX();
console.log("Outside modifyX", x); // Output: 10 (unchanged)

/////////////////////////////  Immutable variables are changed inside the function  /////////////////////////////

// Define an object
var scene = {
  obstacleRadius: 0.1,
  overRelaxation: 1.5,
};
console.log("Before setupScene:", scene);

// Function that modifies the same object
function setupScene() {
  scene.obstacleRadius = 0.15; // Modify existing property
  scene.overRelaxation = 1.9; // Modify existing property
}

setupScene(); // Call function to update scene

console.log("After setupScene:", scene);
