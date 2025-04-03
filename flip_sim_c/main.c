#include <stdio.h>
#include <stdlib.h>
#include "flip.c"  // Include the flip fluid implementation

int main() {
    // Fluid properties
    float density = 1.0f;
    float tankWidth = 10.0f;
    float tankHeight = 10.0f;
    float h = 0.1f;         // Grid spacing
    float r = 0.05f;        // Particle radius
    int numX = 20;          // Number of particles along X-axis
    int numY = 20;          // Number of particles along Y-axis
    int maxParticles = numX * numY;  // Maximum number of particles

    // Create the fluid simulation
    FlipFluid* f = createFlipFluid(density, tankWidth, tankHeight, h, r, maxParticles);
    if (f == NULL) {
        printf("Failed to create fluid.\n");
        return 1;
    }

    f->numParticles = numX * numY;

    // Place particles in a hexagonal grid
    int p = 0;
    float dx = 2 * r;
    float dy = sqrt(3) * r;

    for (int i = 0; i < numX; i++) {
        for (int j = 0; j < numY; j++) {
            f->particlePos[p++] = h + r + dx * i + ((j % 2 == 0) ? 0.0f : r);
            f->particlePos[p++] = h + r + dy * j;
        }
    }

    // Setup grid cells for the tank
    int n = f->fNumY;
    for (int i = 0; i < f->fNumX; i++) {
        for (int j = 0; j < f->fNumY; j++) {
            float s = 1.0f;  // Fluid cell
            if (i == 0 || i == f->fNumX - 1 || j == 0) {
                s = 0.0f;  // Solid boundary
            }
            f->s[i * n + j] = s;
        }
    }

    // Print some particle positions for verification
    printf("First 10 particle positions:\n");
    for (int i = 0; i < 10; i++) {
        printf("Particle %d: (%.3f, %.3f)\n", i, f->particlePos[2 * i], f->particlePos[2 * i + 1]);
    }

    // Cleanup
    freeFlipFluid(f);

    return 0;
}
