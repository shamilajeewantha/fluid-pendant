#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


#define FLUID_CELL 0
#define AIR_CELL 1
#define SOLID_CELL 2


#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))


typedef struct {
    // Fluid properties
    float density;
    int fNumX, fNumY, fNumCells;
    float h, fInvSpacing;
    
    float *u, *v, *du, *dv, *prevU, *prevV, *p, *s;
    int *cellType, *cellColor;

    // Particles
    int maxParticles;
    float *particlePos, *particleVel, *particleDensity;
    float particleRestDensity;

    float particleRadius, pInvSpacing;
    int pNumX, pNumY, pNumCells;
    
    int *numCellParticles, *firstCellParticle, *cellParticleIds;
    int numParticles;
} FlipFluid;

FlipFluid* createFlipFluid(float density, float width, float height, float spacing, float particleRadius, int maxParticles) {
    FlipFluid* fluid = (FlipFluid*)malloc(sizeof(FlipFluid));
    if (fluid == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }

    // Fluid initialization
    fluid->density = density;
    fluid->fNumX = (int)(width / spacing) + 1;
    fluid->fNumY = (int)(height / spacing) + 1;
    fluid->h = fmax(width / fluid->fNumX, height / fluid->fNumY);
    fluid->fInvSpacing = 1.0f / fluid->h;
    fluid->fNumCells = fluid->fNumX * fluid->fNumY;

    fluid->u = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->v = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->du = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->dv = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->prevU = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->prevV = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->p = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->s = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->cellType = (int*)calloc(fluid->fNumCells, sizeof(int));
    fluid->cellColor = (int*)calloc(fluid->fNumCells, sizeof(int));

    // Particle initialization
    fluid->maxParticles = maxParticles;
    fluid->particlePos = (float*)calloc(2 * maxParticles, sizeof(float));
    fluid->particleVel = (float*)calloc(2 * maxParticles, sizeof(float));
    fluid->particleDensity = (float*)calloc(fluid->fNumCells, sizeof(float));
    fluid->particleRestDensity = 0.0f;

    fluid->particleRadius = particleRadius;
    fluid->pInvSpacing = 1.0f / (2.2f * particleRadius);
    fluid->pNumX = (int)(width * fluid->pInvSpacing) + 1;
    fluid->pNumY = (int)(height * fluid->pInvSpacing) + 1;
    fluid->pNumCells = fluid->pNumX * fluid->pNumY;

    fluid->numCellParticles = (int*)calloc(fluid->pNumCells, sizeof(int));
    fluid->firstCellParticle = (int*)calloc(fluid->pNumCells + 1, sizeof(int));
    fluid->cellParticleIds = (int*)calloc(maxParticles, sizeof(int));

    fluid->numParticles = 0;

    return fluid;
}


void freeFlipFluid(FlipFluid* fluid) {
    free(fluid->u);
    free(fluid->v);
    free(fluid->du);
    free(fluid->dv);
    free(fluid->prevU);
    free(fluid->prevV);
    free(fluid->p);
    free(fluid->s);
    free(fluid->cellType);
    free(fluid->cellColor);

    free(fluid->particlePos);
    free(fluid->particleVel);
    free(fluid->particleDensity);
    free(fluid->numCellParticles);
    free(fluid->firstCellParticle);
    free(fluid->cellParticleIds);

    free(fluid);
}


void integrateParticles(FlipFluid* fluid, float dt, float gravity_x, float gravity_y) {
    for (int i = 0; i < fluid->numParticles; i++) {
        fluid->particleVel[2 * i + 1] += dt * gravity_y;
        fluid->particleVel[2 * i] += dt * gravity_x;

        fluid->particlePos[2 * i] += fluid->particleVel[2 * i] * dt;
        fluid->particlePos[2 * i + 1] += fluid->particleVel[2 * i + 1] * dt;
    }
}



void pushParticlesApart(FlipFluid* fluid, int numIters) {
    // Count particles per cell
    for (int i = 0; i < fluid->pNumCells; i++) {
        fluid->numCellParticles[i] = 0;
    }

    for (int i = 0; i < fluid->numParticles; i++) {
        float x = fluid->particlePos[2 * i];
        float y = fluid->particlePos[2 * i + 1];

        int xi = CLAMP((int)(x * fluid->pInvSpacing), 0, fluid->pNumX - 1);
        int yi = CLAMP((int)(y * fluid->pInvSpacing), 0, fluid->pNumY - 1);
        int cellNr = xi * fluid->pNumY + yi;
        fluid->numCellParticles[cellNr]++;
    }

    // Compute partial sums
    int first = 0;
    for (int i = 0; i < fluid->pNumCells; i++) {
        first += fluid->numCellParticles[i];
        fluid->firstCellParticle[i] = first;
    }
    fluid->firstCellParticle[fluid->pNumCells] = first; // Guard

    // Fill particles into cells
    for (int i = 0; i < fluid->numParticles; i++) {
        float x = fluid->particlePos[2 * i];
        float y = fluid->particlePos[2 * i + 1];

        int xi = CLAMP((int)(x * fluid->pInvSpacing), 0, fluid->pNumX - 1);
        int yi = CLAMP((int)(y * fluid->pInvSpacing), 0, fluid->pNumY - 1);
        int cellNr = xi * fluid->pNumY + yi;

        fluid->firstCellParticle[cellNr]--;
        fluid->cellParticleIds[fluid->firstCellParticle[cellNr]] = i;
    }

    // Push particles apart
    float minDist = 2.0f * fluid->particleRadius;
    float minDist2 = minDist * minDist;

    for (int iter = 0; iter < numIters; iter++) {
        for (int i = 0; i < fluid->numParticles; i++) {
            float px = fluid->particlePos[2 * i];
            float py = fluid->particlePos[2 * i + 1];

            int pxi = (int)(px * fluid->pInvSpacing);
            int pyi = (int)(py * fluid->pInvSpacing);
            int x0 = CLAMP(pxi - 1, 0, fluid->pNumX - 1);
            int y0 = CLAMP(pyi - 1, 0, fluid->pNumY - 1);
            int x1 = CLAMP(pxi + 1, 0, fluid->pNumX - 1);
            int y1 = CLAMP(pyi + 1, 0, fluid->pNumY - 1);

            for (int xi = x0; xi <= x1; xi++) {
                for (int yi = y0; yi <= y1; yi++) {
                    int cellNr = xi * fluid->pNumY + yi;
                    int first = fluid->firstCellParticle[cellNr];
                    int last = fluid->firstCellParticle[cellNr + 1];

                    for (int j = first; j < last; j++) {
                        int id = fluid->cellParticleIds[j];
                        if (id == i) continue;

                        float qx = fluid->particlePos[2 * id];
                        float qy = fluid->particlePos[2 * id + 1];

                        float dx = qx - px;
                        float dy = qy - py;
                        float d2 = dx * dx + dy * dy;
                        if (d2 > minDist2 || d2 == 0.0f) continue;

                        float d = sqrtf(d2);
                        float s = (0.5f * (minDist - d)) / d;
                        dx *= s;
                        dy *= s;

                        fluid->particlePos[2 * i] -= dx;
                        fluid->particlePos[2 * i + 1] -= dy;
                        fluid->particlePos[2 * id] += dx;
                        fluid->particlePos[2 * id + 1] += dy;
                    }
                }
            }
        }
    }
}



void updateParticleDensity(FlipFluid* fluid) {
    int n = fluid->fNumY;
    float h = fluid->h;
    float h1 = fluid->fInvSpacing;
    float h2 = 0.5f * h;

    float* d = fluid->particleDensity;

    // Initialize density array to zero
    for (int i = 0; i < fluid->fNumCells; i++) {
        d[i] = 0.0f;
    }

    // Compute density contributions from particles
    for (int i = 0; i < fluid->numParticles; i++) {
        float x = fluid->particlePos[2 * i];
        float y = fluid->particlePos[2 * i + 1];

        // Clamp x and y to prevent out-of-bounds errors
        x = fmaxf(h, fminf(x, (fluid->fNumX - 1) * h));
        y = fmaxf(h, fminf(y, (fluid->fNumY - 1) * h));

        int x0 = (int)((x - h2) * h1);
        float tx = ((x - h2) - x0 * h) * h1;
        int x1 = fmin(x0 + 1, fluid->fNumX - 2);

        int y0 = (int)((y - h2) * h1);
        float ty = ((y - h2) - y0 * h) * h1;
        int y1 = fmin(y0 + 1, fluid->fNumY - 2);

        float sx = 1.0f - tx;
        float sy = 1.0f - ty;

        if (x0 < fluid->fNumX && y0 < fluid->fNumY) d[x0 * n + y0] += sx * sy;
        if (x1 < fluid->fNumX && y0 < fluid->fNumY) d[x1 * n + y0] += tx * sy;
        if (x1 < fluid->fNumX && y1 < fluid->fNumY) d[x1 * n + y1] += tx * ty;
        if (x0 < fluid->fNumX && y1 < fluid->fNumY) d[x0 * n + y1] += sx * ty;
    }

    // Set the reference density if it has not been initialized
    if (fluid->particleRestDensity == 0.0f) {
        float sum = 0.0f;
        int numFluidCells = 0;

        for (int i = 0; i < fluid->fNumCells; i++) {
            if (fluid->cellType[i] == FLUID_CELL) {
                sum += d[i];
                numFluidCells++;
            }
        }

        if (numFluidCells > 0) {
            fluid->particleRestDensity = sum / numFluidCells;
        }
    }
}



void transferVelocities(FlipFluid* fluid, int toGrid, float flipRatio) {
    int n = fluid->fNumY;
    float h = fluid->h;
    float h1 = fluid->fInvSpacing;
    float h2 = 0.5f * h;

    if (toGrid) {
        memcpy(fluid->prevU, fluid->u, fluid->fNumCells * sizeof(float));
        memcpy(fluid->prevV, fluid->v, fluid->fNumCells * sizeof(float));

        memset(fluid->du, 0, fluid->fNumCells * sizeof(float));
        memset(fluid->dv, 0, fluid->fNumCells * sizeof(float));
        memset(fluid->u, 0, fluid->fNumCells * sizeof(float));
        memset(fluid->v, 0, fluid->fNumCells * sizeof(float));

        for (int i = 0; i < fluid->fNumCells; i++) {
            fluid->cellType[i] = (fluid->s[i] == 0.0f) ? SOLID_CELL : AIR_CELL;
        }

        for (int i = 0; i < fluid->numParticles; i++) {
            float x = fluid->particlePos[2 * i];
            float y = fluid->particlePos[2 * i + 1];
            int xi = fmaxf(0, fminf((int)(x * h1), fluid->fNumX - 1));
            int yi = fmaxf(0, fminf((int)(y * h1), fluid->fNumY - 1));
            int cellNr = xi * n + yi;
            if (fluid->cellType[cellNr] == AIR_CELL) {
                fluid->cellType[cellNr] = FLUID_CELL;
            }
        }
    }

    for (int component = 0; component < 2; component++) {
        float dx = (component == 0) ? 0.0f : h2;
        float dy = (component == 0) ? h2 : 0.0f;

        float* f = (component == 0) ? fluid->u : fluid->v;
        float* prevF = (component == 0) ? fluid->prevU : fluid->prevV;
        float* d = (component == 0) ? fluid->du : fluid->dv;

        for (int i = 0; i < fluid->numParticles; i++) {
            float x = fluid->particlePos[2 * i];
            float y = fluid->particlePos[2 * i + 1];

            x = fmaxf(h, fminf(x, (fluid->fNumX - 1) * h));
            y = fmaxf(h, fminf(y, (fluid->fNumY - 1) * h));

            int x0 = fmin((int)((x - dx) * h1), fluid->fNumX - 2);
            float tx = ((x - dx) - x0 * h) * h1;
            int x1 = fmin(x0 + 1, fluid->fNumX - 2);

            int y0 = fmin((int)((y - dy) * h1), fluid->fNumY - 2);
            float ty = ((y - dy) - y0 * h) * h1;
            int y1 = fmin(y0 + 1, fluid->fNumY - 2);

            float sx = 1.0f - tx;
            float sy = 1.0f - ty;

            float d0 = sx * sy;
            float d1 = tx * sy;
            float d2 = tx * ty;
            float d3 = sx * ty;

            int nr0 = x0 * n + y0;
            int nr1 = x1 * n + y0;
            int nr2 = x1 * n + y1;
            int nr3 = x0 * n + y1;

            if (toGrid) {
                float pv = fluid->particleVel[2 * i + component];
                f[nr0] += pv * d0;
                d[nr0] += d0;
                f[nr1] += pv * d1;
                d[nr1] += d1;
                f[nr2] += pv * d2;
                d[nr2] += d2;
                f[nr3] += pv * d3;
                d[nr3] += d3;
            } else {
                int offset = (component == 0) ? n : 1;
                float valid0 = (fluid->cellType[nr0] != AIR_CELL || fluid->cellType[nr0 - offset] != AIR_CELL) ? 1.0f : 0.0f;
                float valid1 = (fluid->cellType[nr1] != AIR_CELL || fluid->cellType[nr1 - offset] != AIR_CELL) ? 1.0f : 0.0f;
                float valid2 = (fluid->cellType[nr2] != AIR_CELL || fluid->cellType[nr2 - offset] != AIR_CELL) ? 1.0f : 0.0f;
                float valid3 = (fluid->cellType[nr3] != AIR_CELL || fluid->cellType[nr3 - offset] != AIR_CELL) ? 1.0f : 0.0f;

                float v = fluid->particleVel[2 * i + component];
                float dSum = valid0 * d0 + valid1 * d1 + valid2 * d2 + valid3 * d3;

                if (dSum > 0.0f) {
                    float picV = (valid0 * d0 * f[nr0] + valid1 * d1 * f[nr1] + valid2 * d2 * f[nr2] + valid3 * d3 * f[nr3]) / dSum;
                    float corr = (valid0 * d0 * (f[nr0] - prevF[nr0]) +
                                  valid1 * d1 * (f[nr1] - prevF[nr1]) +
                                  valid2 * d2 * (f[nr2] - prevF[nr2]) +
                                  valid3 * d3 * (f[nr3] - prevF[nr3])) / dSum;
                    float flipV = v + corr;

                    fluid->particleVel[2 * i + component] = (1.0f - flipRatio) * picV + flipRatio * flipV;
                }
            }
        }

        if (toGrid) {
            for (int i = 0; i < fluid->fNumCells; i++) {
                if (d[i] > 0.0f) {
                    f[i] /= d[i];
                }
            }

            // Restore solid cells
            for (int i = 0; i < fluid->fNumX; i++) {
                for (int j = 0; j < fluid->fNumY; j++) {
                    int solid = (fluid->cellType[i * n + j] == SOLID_CELL);
                    if (solid || (i > 0 && fluid->cellType[(i - 1) * n + j] == SOLID_CELL)) {
                        fluid->u[i * n + j] = fluid->prevU[i * n + j];
                    }
                    if (solid || (j > 0 && fluid->cellType[i * n + j - 1] == SOLID_CELL)) {
                        fluid->v[i * n + j] = fluid->prevV[i * n + j];
                    }
                }
            }
        }
    }
}

////////////// after gpt lost /////////

void solveIncompressibility(FlipFluid* fluid, int numIters, float dt, float overRelaxation, int compensateDrift) {
    // Initialize pressure and set previous velocities
    memset(fluid->p, 0, fluid->fNumCells * sizeof(float));
    memcpy(fluid->prevU, fluid->u, fluid->fNumCells * sizeof(float));
    memcpy(fluid->prevV, fluid->v, fluid->fNumCells * sizeof(float));

    int n = fluid->fNumY;
    float cp = (fluid->density * fluid->h) / dt;

    // Iterate over the number of iterations for pressure solver
    for (int iter = 0; iter < numIters; iter++) {
        for (int i = 1; i < fluid->fNumX - 1; i++) {
            for (int j = 1; j < fluid->fNumY - 1; j++) {
                int center = i * n + j;
                if (fluid->cellType[center] != FLUID_CELL) continue;

                int left = (i - 1) * n + j;
                int right = (i + 1) * n + j;
                int bottom = i * n + j - 1;
                int top = i * n + j + 1;

                float s = fluid->s[center];
                float sx0 = fluid->s[left];
                float sx1 = fluid->s[right];
                float sy0 = fluid->s[bottom];
                float sy1 = fluid->s[top];

                // Sum of s values
                s = sx0 + sx1 + sy0 + sy1;
                if (s == 0.0f) continue;

                // Calculate divergence
                float div = fluid->u[right] - fluid->u[center] + fluid->v[top] - fluid->v[center];

                // Compensate drift if required
                if (fluid->particleRestDensity > 0.0f && compensateDrift) {
                    float k = 1.0f;
                    float compression = fluid->particleDensity[center] - fluid->particleRestDensity;
                    if (compression > 0.0f) {
                        div -= k * compression;
                    }
                }

                // Compute pressure
                float p = -div / s;
                p *= overRelaxation;
                fluid->p[center] += cp * p;

                // Update velocities based on pressure gradient
                fluid->u[center] -= sx0 * p;
                fluid->u[right] += sx1 * p;
                fluid->v[center] -= sy0 * p;
                fluid->v[top] += sy1 * p;
            }
        }
    }
}


void updateCellColors(FlipFluid* fluid) {
    // Set all cell colors to black
    memset(fluid->cellColor, 0, fluid->fNumCells * sizeof(int));

    // Iterate over all cells
    for (int i = 0; i < fluid->fNumCells; i++) {
        if (fluid->cellType[i] == FLUID_CELL) {
            fluid->cellColor[i] = 1; // Set to green component (1)
        }
    }
}

void simulate(FlipFluid* fluid, 
    float dt, 
    float gravity_x, 
    float gravity_y, 
    float flipRatio, 
    int numPressureIters, 
    int numParticleIters, 
    float overRelaxation, 
    int compensateDrift, 
    int separateParticles, 
    float obstacleX, 
    float obstacleY, 
    float obstacleRadius) 
{
int numSubSteps = 1;
float sdt = dt / numSubSteps;

for (int step = 0; step < numSubSteps; step++) {
// Integrate particles
integrateParticles(fluid, sdt, gravity_x, gravity_y);

// Separate particles if needed
if (separateParticles) {
  pushParticlesApart(fluid, numParticleIters);
}

// Transfer velocities to grid
transferVelocities(fluid, 1, flipRatio);

// Update particle density
updateParticleDensity(fluid);

// Solve for incompressibility
solveIncompressibility(fluid, numPressureIters, sdt, overRelaxation, compensateDrift);

// Transfer velocities back
transferVelocities(fluid, 0, flipRatio);
}

// Update cell colors
updateCellColors(fluid);
}


