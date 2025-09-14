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
} FlipFluid;

#endif
