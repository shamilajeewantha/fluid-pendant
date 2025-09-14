#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "flip_utils.h"
#include "scene.h"
#include "flip_fluid.h"

// Setup a scene with FlipFluid
void setupScene(Scene *scene, float simWidth, float simHeight) {
    if (!scene) return;

    int res = 9;
    float tankHeight = simHeight;
    float tankWidth  = simHeight;
    float h = tankHeight / res;
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
    f->fNumX = numX;
    f->fNumY = numY;
    f->density = density;
    f->tankWidth = tankWidth;
    f->tankHeight = tankHeight;
    f->h = h;
    f->r = r;

    f->particlePos = (float*)malloc(sizeof(float) * maxParticles * 2);
    f->s = (float*)malloc(sizeof(float) * numX * numY);

    // Place particles in hexagonal grid
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
            if (i == 0 || i == f->fNumX - 1 || j == 0) s = 0.0f;
            f->s[i * n + j] = s;
        }
    }

    scene->fluid = f;

    printf("Scene setup complete: %d particles, grid %dx%d\n", maxParticles, numX, numY);
}
