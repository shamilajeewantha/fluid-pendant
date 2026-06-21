#ifndef FLIP_FLUID_H
#define FLIP_FLUID_H

// Display grid resolution: 1:1 with the 16-row x 15-column charlieplexed LED matrix.
#define GRID_X 15  // columns
#define GRID_Y 16  // rows

// The solver's own internal grid is padded one cell beyond the display on every walled
// side (left, right, bottom — see flip_utils.c's setupScene() wall setup), so those
// structurally-solid wall cells fall outside the visible GRID_X x GRID_Y window instead
// of being its outermost ring (which would otherwise make those display cells permanently
// unable to show fluid, since wall cells never become FLUID_CELL). The top has no wall,
// so it isn't padded. See research.md Decision 7.
#define PAD_FNUM_X (GRID_X + 2)
#define PAD_FNUM_Y (GRID_Y + 1)

typedef struct {
    float *particlePos;   // x,y positions
    float *s;             // grid solid/fluid flags
    int numParticles;
    int maxParticles;
    int fNumX, fNumY;
    float density;
    float tankWidth;
    float tankHeight;
    float h;              // cell size
    float r;              // particle radius
    float *cellColor;     // add this
} FlipFluid;

// Initialize fluid
void initFlipFluid(FlipFluid *f, int maxParticles);

// Simulate fluid
void simulateFlipFluid(
    FlipFluid *f,
    float dt,
    float gravity_x,
    float gravity_y,
    float flipRatio,
    int numPressureIters,
    int numParticleIters,
    float overRelaxation,
    int compensateDrift,
    int separateParticles
);

#endif
