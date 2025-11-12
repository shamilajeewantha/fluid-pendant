#ifndef FLIP_FLUID_H
#define FLIP_FLUID_H

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
    float *cellColor;     // 10x10 grid for display (row-major)
} FlipFluid;

// Simulate fluid
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
);

#endif
