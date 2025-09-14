#include "flip_fluid.h"
#include "scene.h"
#include <stdlib.h>
#include <string.h>

#define GRID_X 10
#define GRID_Y 10

// We'll create a simple cell color array (0..1 floats)
static float cellColor[GRID_X * GRID_Y]; // 10x10 grid
static int rowIndex = 0;                 // green row position

// This is the simulate function
void simulateFlipFluid(
    FlipFluid *f,
    float dt,
    double gravity_x,
    double gravity_y,
    float flipRatio,
    int numPressureIters,
    int numParticleIters,
    float overRelaxation,
    int compensateDrift,
    int separateParticles
) {
    // Clear previous colors
    for (int i = 0; i < GRID_X * GRID_Y; i++)
        cellColor[i] = 0.0f;

    // Set one row to green
    for (int col = 0; col < GRID_X; col++) {
        int idx = rowIndex * GRID_X + col;
        if (idx < GRID_X * GRID_Y)
            cellColor[idx] = 1.0f; // green intensity
    }

    // Move row down
    rowIndex = (rowIndex + 1) % GRID_Y;

    // Copy colors into the fluid structure
    if (f) {
        f->s = f->s; // unused here
        f->particlePos = f->particlePos; // unused
        // Copy cellColor data to fluid structure
        if (f->cellColor) {
            for (int i = 0; i < GRID_X * GRID_Y; i++) {
                f->cellColor[i] = cellColor[i];
            }
        }
    }
}
