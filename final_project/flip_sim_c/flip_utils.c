#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "flip_utils.h"
#include "scene.h"
#include "flip_fluid.h"

// Setup a scene with FlipFluid
void setupScene(Scene *scene) {
    if (!scene) return;
    // Solver grid is padded beyond the GRID_X x GRID_Y display (flip_fluid.h, research.md
    // Decision 7); fNumX/fNumY come out to PAD_FNUM_X x PAD_FNUM_Y below, so resX/resY are
    // one less than those padded targets. Cells stay square (same h both directions) by
    // deriving tankWidth from the same cell size h as tankHeight.
    int resX = PAD_FNUM_X - 1;
    int resY = PAD_FNUM_Y - 1;
    float simHeight = 3.0f;
    float tankHeight = simHeight;
    float h = tankHeight / resY;
    float tankWidth = resX * h;
    float density = 10.0f;

    float relWaterHeight = 0.8f;
    float relWaterWidth  = 0.8f;

    float r  = 0.3f * h;
    float dx = 2.0f * r;
    float dy = (sqrtf(3.0f) / 2.0f) * dx;

    int numX = (int)((relWaterWidth * tankWidth - 2.0f * h - 2.0f * r) / dx);
    int numY = (int)((relWaterHeight * tankHeight - 2.0f * h - 2.0f * r) / dy);
    int maxParticles = numX * numY;

    // Allocate fluid
    FlipFluid *f = (FlipFluid*)malloc(sizeof(FlipFluid));
    f->numParticles = maxParticles;
    f->maxParticles = maxParticles;
    // Grid resolution is fixed to the padded solver size directly (not re-derived via
    // tankWidth/h, since that floor-division round-trip risks off-by-one errors
    // from floating-point rounding given tankWidth was itself built from h).
    f->fNumX = PAD_FNUM_X;
    f->fNumY = PAD_FNUM_Y;
    f->density = density;
    f->tankWidth = tankWidth;
    f->tankHeight = tankHeight;
    f->h = h;
    f->r = r;

    f->particlePos = (float*)malloc(sizeof(float) * maxParticles * 2);
    f->s = (float*)malloc(sizeof(float) * f->fNumX * f->fNumY);
    f->cellColor = (float*)malloc(sizeof(float) * GRID_X * GRID_Y); // 15x16 grid for cellColor

    // Place particles in hexagonal grid (fits within tank using numX/numY)
    int p = 0;
    for (int i = 0; i < numX; i++) {
        for (int j = 0; j < numY; j++) {
            f->particlePos[p++] = h + r + dx * i + ((j % 2 == 0) ? 0.0f : r);
            f->particlePos[p++] = h + r + dy * j;
        }
    }

    // Setup grid cells
    int n = f->fNumY;
    for (int i = 0; i < f->fNumX; i++) {
        for (int j = 0; j < f->fNumY; j++) {
            float s = 1.0f;
            // Walls on left, bottom, right. Top remains fluid – matches JS tank setup
            if (i == 0 || i == f->fNumX - 1 || j == 0) s = 0.0f;
            f->s[i * n + j] = s;
        }
    }

    scene->fluid = f;

    printf("Scene setup complete: %d particles, grid %dx%d\n", maxParticles, numX, numY);
}
