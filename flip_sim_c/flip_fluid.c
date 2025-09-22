#include "flip_fluid.h"
#include "scene.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static inline float ff_maxf(float a, float b){return a > b ? a : b;}
static inline float ff_minf(float a, float b){return a < b ? a : b;}
static inline float ff_clamp(float x, float lo, float hi){return x < lo ? lo : (x > hi ? hi : x);} 

#define GRID_X 10
#define GRID_Y 10

// Internal, lazily allocated state for velocities. We intentionally keep this
// local to this file to avoid changing public headers.
static float *g_particleVel = NULL; // size 2 * maxParticles
static int g_alloc_for_particles = 0;

// --- Grid-based state (PIC/FLIP fields) ---
static int g_fNumX = 0, g_fNumY = 0, g_fNumCells = 0;
static float *g_u = NULL, *g_v = NULL;              // grid velocities
static float *g_du = NULL, *g_dv = NULL;            // weights accumulation
static float *g_prevU = NULL, *g_prevV = NULL;      // previous velocities
static float *g_p = NULL;                           // pressure
static float *g_particleDensity = NULL;             // per-cell particle density
static int   *g_cellType = NULL;                    // 0 fluid, 1 air, 2 solid
static float g_particleRestDensity = 0.0f;          // computed once from fluid cells

// --- Particle spatial hash grid ---
static float g_pInvSpacing = 0.0f;
static int g_pNumX = 0, g_pNumY = 0, g_pNumCells = 0;
static int *g_numCellParticles = NULL;
static int *g_firstCellParticle = NULL; // length pNumCells+1
static int *g_cellParticleIds = NULL;   // length maxParticles

enum { FLUID_CELL = 0, AIR_CELL = 1, SOLID_CELL = 2 };

static void ensure_grid_alloc(const FlipFluid *f) {
    if (!f) return;
    const int wantX = f->fNumX;
    const int wantY = f->fNumY;
    const int wantCells = wantX * wantY;
    if (wantX == g_fNumX && wantY == g_fNumY && g_u && g_v && g_p) return;
    free(g_u); free(g_v); free(g_du); free(g_dv); free(g_prevU); free(g_prevV);
    free(g_p); free(g_particleDensity); free(g_cellType);
    g_u = (float*)calloc((size_t)wantCells, sizeof(float));
    g_v = (float*)calloc((size_t)wantCells, sizeof(float));
    g_du = (float*)calloc((size_t)wantCells, sizeof(float));
    g_dv = (float*)calloc((size_t)wantCells, sizeof(float));
    g_prevU = (float*)calloc((size_t)wantCells, sizeof(float));
    g_prevV = (float*)calloc((size_t)wantCells, sizeof(float));
    g_p = (float*)calloc((size_t)wantCells, sizeof(float));
    g_particleDensity = (float*)calloc((size_t)wantCells, sizeof(float));
    g_cellType = (int*)calloc((size_t)wantCells, sizeof(int));
    g_fNumX = wantX; g_fNumY = wantY; g_fNumCells = wantCells;
}

static void ensure_particle_hash_alloc(const FlipFluid *f) {
    if (!f) return;
    const float particleRadius = f->r > 1e-6f ? f->r : 1e-6f;
    g_pInvSpacing = 1.0f / (2.2f * particleRadius);
    g_pNumX = (int)floorf(f->tankWidth  * g_pInvSpacing) + 1;
    g_pNumY = (int)floorf(f->tankHeight * g_pInvSpacing) + 1;
    g_pNumCells = g_pNumX * g_pNumY;
    free(g_numCellParticles); free(g_firstCellParticle); free(g_cellParticleIds);
    g_numCellParticles = (int*)calloc((size_t)g_pNumCells, sizeof(int));
    g_firstCellParticle = (int*)calloc((size_t)g_pNumCells + 1u, sizeof(int));
    g_cellParticleIds = (int*)calloc((size_t)f->maxParticles, sizeof(int));
}

static void ensure_alloc_particle_vel(const FlipFluid *f) {
    if (!f) return;
    if (g_alloc_for_particles >= f->maxParticles && g_particleVel) return;
    free(g_particleVel);
    g_particleVel = (float*)calloc((size_t)f->maxParticles * 2u, sizeof(float));
    g_alloc_for_particles = f->maxParticles;
}

static void integrate_particles(FlipFluid *f, float dt, double gx, double gy) {
    if (!f || !f->particlePos || f->numParticles <= 0) return;
    ensure_alloc_particle_vel(f);
    const float gxf = (float)gx;
    const float gyf = (float)gy;
    for (int i = 0; i < f->numParticles; i++) {
        g_particleVel[2*i + 0] += dt * gxf;
        g_particleVel[2*i + 1] += dt * gyf;
        f->particlePos[2*i + 0] += g_particleVel[2*i + 0] * dt;
        f->particlePos[2*i + 1] += g_particleVel[2*i + 1] * dt;
    }
}

// Simple O(n^2) particle separation to reduce overlap. This is a coarse
// substitute for the cell-based neighbor search in JS.
static void push_particles_apart(FlipFluid *f, int numIters) {
    if (!f || !f->particlePos || f->numParticles <= 1) return;
    ensure_particle_hash_alloc(f);
    const float minDist = 2.0f * f->r;
    const float minDist2 = minDist * minDist;
    for (int iter = 0; iter < numIters; iter++) {
        memset(g_numCellParticles, 0, sizeof(int) * (size_t)g_pNumCells);
        for (int i = 0; i < f->numParticles; i++) {
            float x = f->particlePos[2*i + 0];
            float y = f->particlePos[2*i + 1];
            int xi = (int)ff_clamp(floorf(x * g_pInvSpacing), 0.0f, (float)(g_pNumX - 1));
            int yi = (int)ff_clamp(floorf(y * g_pInvSpacing), 0.0f, (float)(g_pNumY - 1));
            int cell = xi * g_pNumY + yi;
            g_numCellParticles[cell]++;
        }
        int first = 0;
        for (int c = 0; c < g_pNumCells; c++) {
            first += g_numCellParticles[c];
            g_firstCellParticle[c] = first;
        }
        g_firstCellParticle[g_pNumCells] = first;
        for (int i = 0; i < f->numParticles; i++) {
            float x = f->particlePos[2*i + 0];
            float y = f->particlePos[2*i + 1];
            int xi = (int)ff_clamp(floorf(x * g_pInvSpacing), 0.0f, (float)(g_pNumX - 1));
            int yi = (int)ff_clamp(floorf(y * g_pInvSpacing), 0.0f, (float)(g_pNumY - 1));
            int cell = xi * g_pNumY + yi;
            g_firstCellParticle[cell]--;
            g_cellParticleIds[g_firstCellParticle[cell]] = i;
        }
        for (int i = 0; i < f->numParticles; i++) {
            float px = f->particlePos[2*i + 0];
            float py = f->particlePos[2*i + 1];
            int pxi = (int)floorf(px * g_pInvSpacing);
            int pyi = (int)floorf(py * g_pInvSpacing);
            int x0 = (pxi - 1) < 0 ? 0 : (pxi - 1);
            int y0 = (pyi - 1) < 0 ? 0 : (pyi - 1);
            int x1 = (pxi + 1) > (g_pNumX - 1) ? (g_pNumX - 1) : (pxi + 1);
            int y1 = (pyi + 1) > (g_pNumY - 1) ? (g_pNumY - 1) : (pyi + 1);
            for (int xi = x0; xi <= x1; xi++) {
                for (int yi = y0; yi <= y1; yi++) {
                    int cell = xi * g_pNumY + yi;
                    int firstIdx = g_firstCellParticle[cell];
                    int lastIdx  = g_firstCellParticle[cell + 1];
                    for (int jIdx = firstIdx; jIdx < lastIdx; jIdx++) {
                        int id = g_cellParticleIds[jIdx];
                        if (id == i) continue;
                        float qx = f->particlePos[2*id + 0];
                        float qy = f->particlePos[2*id + 1];
                        float dx = qx - px;
                        float dy = qy - py;
                        float d2 = dx*dx + dy*dy;
                        if (d2 > minDist2 || d2 == 0.0f) continue;
                        float d = sqrtf(d2);
                        float s = (0.5f * (minDist - d)) / d;
                        dx *= s; dy *= s;
                        f->particlePos[2*i + 0] -= dx;
                        f->particlePos[2*i + 1] -= dy;
                        f->particlePos[2*id + 0] += dx;
                        f->particlePos[2*id + 1] += dy;
                    }
                }
            }
        }
    }
}

static void handle_collisions(FlipFluid *f) {
    if (!f || !f->particlePos || f->numParticles <= 0) return;
    const float h = f->h;
    const float r = f->r;
    const float minX = h + r;
    const float maxX = (f->fNumX - 1) * h - r; // match JS: (fNumX-1)*h - r
    const float minY = h + r;
    const float maxY = (f->fNumY - 1) * h - r; // match JS: (fNumY-1)*h - r
    for (int i = 0; i < f->numParticles; i++) {
        float x = f->particlePos[2*i + 0];
        float y = f->particlePos[2*i + 1];
        if (x < minX) { x = minX; g_particleVel[2*i + 0] = 0.0f; }
        if (x > maxX) { x = maxX; g_particleVel[2*i + 0] = 0.0f; }
        if (y < minY) { y = minY; g_particleVel[2*i + 1] = 0.0f; }
        if (y > maxY) { y = maxY; g_particleVel[2*i + 1] = 0.0f; }
        f->particlePos[2*i + 0] = x;
        f->particlePos[2*i + 1] = y;
    }
}

// --- Particle density to grid (bilinear) ---
static void update_particle_density(const FlipFluid *f) {
    if (!f || !f->particlePos || !g_particleDensity) return;
    const int n = g_fNumY;
    const float h = f->h;
    const float h1 = 1.0f / ff_maxf(h, 1e-6f);
    const float h2 = 0.5f * h;
    memset(g_particleDensity, 0, sizeof(float) * (size_t)g_fNumCells);
    for (int i = 0; i < f->numParticles; i++) {
        float x = f->particlePos[2*i + 0];
        float y = f->particlePos[2*i + 1];
        x = ff_clamp(x, h, (f->fNumX - 1) * h);
        y = ff_clamp(y, h, (f->fNumY - 1) * h);
        int x0 = (int)floorf((x - h2) * h1);
        float tx = (x - h2 - x0 * h) * h1;
        int x1 = (x0 + 1 < f->fNumX - 1) ? (x0 + 1) : (f->fNumX - 2);
        int y0 = (int)floorf((y - h2) * h1);
        float ty = (y - h2 - y0 * h) * h1;
        int y1 = (y0 + 1 < f->fNumY - 1) ? (y0 + 1) : (f->fNumY - 2);
        float sx = 1.0f - tx;
        float sy = 1.0f - ty;
        int nr0 = x0 * n + y0;
        int nr1 = x1 * n + y0;
        int nr2 = x1 * n + y1;
        int nr3 = x0 * n + y1;
        if (x0 < f->fNumX && y0 < f->fNumY) g_particleDensity[nr0] += sx * sy;
        if (x1 < f->fNumX && y0 < f->fNumY) g_particleDensity[nr1] += tx * sy;
        if (x1 < f->fNumX && y1 < f->fNumY) g_particleDensity[nr2] += tx * ty;
        if (x0 < f->fNumX && y1 < f->fNumY) g_particleDensity[nr3] += sx * ty;
    }

    // Compute rest density once (reference) from current fluid cells
    if (g_particleRestDensity == 0.0f && g_cellType) {
        float sum = 0.0f;
        int numFluidCells = 0;
        for (int i = 0; i < g_fNumCells; i++) {
            if (g_cellType[i] == FLUID_CELL) { sum += g_particleDensity[i]; numFluidCells++; }
        }
        if (numFluidCells > 0) g_particleRestDensity = sum / (float)numFluidCells;
    }
}

// --- Transfer velocities between particles and grid ---
static void transfer_velocities(FlipFluid *f, int toGrid, float flipRatio) {
    if (!f || !f->particlePos) return;
    const int n = g_fNumY;
    const float h = f->h;
    const float h1 = 1.0f / ff_maxf(h, 1e-6f);
    const float h2 = 0.5f * h;
    if (toGrid) {
        memcpy(g_prevU, g_u, sizeof(float) * (size_t)g_fNumCells);
        memcpy(g_prevV, g_v, sizeof(float) * (size_t)g_fNumCells);
        memset(g_du, 0, sizeof(float) * (size_t)g_fNumCells);
        memset(g_dv, 0, sizeof(float) * (size_t)g_fNumCells);
        memset(g_u, 0, sizeof(float) * (size_t)g_fNumCells);
        memset(g_v, 0, sizeof(float) * (size_t)g_fNumCells);
        for (int i = 0; i < g_fNumCells; i++) g_cellType[i] = (f->s && f->s[i] == 0.0f) ? SOLID_CELL : AIR_CELL;
    }
    for (int component = 0; component < 2; component++) {
        const float dx = (component == 0) ? 0.0f : h2;
        const float dy = (component == 0) ? h2  : 0.0f;
        float *F   = (component == 0) ? g_u : g_v;
        float *pF  = (component == 0) ? g_prevU : g_prevV;
        float *acc = (component == 0) ? g_du : g_dv;
        for (int i = 0; i < f->numParticles; i++) {
            float x = ff_clamp(f->particlePos[2*i + 0], h, (f->fNumX - 1) * h);
            float y = ff_clamp(f->particlePos[2*i + 1], h, (f->fNumY - 1) * h);
            int x0 = (int)ff_minf(floorf((x - dx) * h1), (float)(f->fNumX - 2));
            float tx = (x - dx - x0 * h) * h1;
            int x1 = (x0 + 1 < f->fNumX - 1) ? (x0 + 1) : (f->fNumX - 2);
            int y0 = (int)ff_minf(floorf((y - dy) * h1), (float)(f->fNumY - 2));
            float ty = (y - dy - y0 * h) * h1;
            int y1 = (y0 + 1 < f->fNumY - 1) ? (y0 + 1) : (f->fNumY - 2);
            float sx = 1.0f - tx, sy = 1.0f - ty;
            float d0 = sx * sy, d1 = tx * sy, d2 = tx * ty, d3 = sx * ty;
            int nr0 = x0 * n + y0;
            int nr1 = x1 * n + y0;
            int nr2 = x1 * n + y1;
            int nr3 = x0 * n + y1;
            if (toGrid) {
                float pv = g_particleVel[2*i + component];
                F[nr0] += pv * d0; acc[nr0] += d0;
                F[nr1] += pv * d1; acc[nr1] += d1;
                F[nr2] += pv * d2; acc[nr2] += d2;
                F[nr3] += pv * d3; acc[nr3] += d3;
                int xi = (int)ff_clamp(floorf(f->particlePos[2*i + 0] * h1), 0.0f, (float)(f->fNumX - 1));
                int yi = (int)ff_clamp(floorf(f->particlePos[2*i + 1] * h1), 0.0f, (float)(f->fNumY - 1));
                int cell = xi * n + yi;
                if (g_cellType[cell] == AIR_CELL) g_cellType[cell] = FLUID_CELL;
            } else {
                int offset = (component == 0) ? n : 1;
                float valid0 = 0.0f, valid1 = 0.0f, valid2 = 0.0f, valid3 = 0.0f;
                int i0 = nr0 - offset; if (i0 >= 0 && nr0 >= 0 && nr0 < g_fNumCells)
                    valid0 = (g_cellType[nr0] != AIR_CELL || g_cellType[i0] != AIR_CELL) ? 1.0f : 0.0f;
                int i1 = nr1 - offset; if (i1 >= 0 && nr1 >= 0 && nr1 < g_fNumCells)
                    valid1 = (g_cellType[nr1] != AIR_CELL || g_cellType[i1] != AIR_CELL) ? 1.0f : 0.0f;
                int i2 = nr2 - offset; if (i2 >= 0 && nr2 >= 0 && nr2 < g_fNumCells)
                    valid2 = (g_cellType[nr2] != AIR_CELL || g_cellType[i2] != AIR_CELL) ? 1.0f : 0.0f;
                int i3 = nr3 - offset; if (i3 >= 0 && nr3 >= 0 && nr3 < g_fNumCells)
                    valid3 = (g_cellType[nr3] != AIR_CELL || g_cellType[i3] != AIR_CELL) ? 1.0f : 0.0f;
                float wsum = valid0 * d0 + valid1 * d1 + valid2 * d2 + valid3 * d3;
                if (wsum > 0.0f) {
                    float picV = (
                        valid0 * d0 * F[nr0] +
                        valid1 * d1 * F[nr1] +
                        valid2 * d2 * F[nr2] +
                        valid3 * d3 * F[nr3]
                    ) / wsum;
                    float corr = (
                        valid0 * d0 * (F[nr0] - pF[nr0]) +
                        valid1 * d1 * (F[nr1] - pF[nr1]) +
                        valid2 * d2 * (F[nr2] - pF[nr2]) +
                        valid3 * d3 * (F[nr3] - pF[nr3])
                    ) / wsum;
                    float flipV = g_particleVel[2*i + component] + corr;
                    g_particleVel[2*i + component] = (1.0f - flipRatio) * picV + flipRatio * flipV;
                }
            }
        }
        if (toGrid) {
            for (int i = 0; i < g_fNumCells; i++) if (acc[i] > 0.0f) F[i] /= acc[i];
            for (int i = 0; i < f->fNumX; i++) {
                for (int j = 0; j < f->fNumY; j++) {
                    int center = i * n + j;
                    int left = (i - 1) * n + j;
                    int bottom = i * n + j - 1;
                    int isSolid = (g_cellType[center] == SOLID_CELL);
                    if (isSolid || (i > 0 && g_cellType[left] == SOLID_CELL)) g_u[center] = g_prevU[center];
                    if (isSolid || (j > 0 && g_cellType[bottom] == SOLID_CELL)) g_v[center] = g_prevV[center];
                }
            }
        }
    }
}

// --- Pressure solve ---
static void solve_incompressibility(FlipFluid *f, int numIters, float dt, float overRelaxation, int compensateDrift) {
    if (!f) return;
    memset(g_p, 0, sizeof(float) * (size_t)g_fNumCells);
    memcpy(g_prevU, g_u, sizeof(float) * (size_t)g_fNumCells);
    memcpy(g_prevV, g_v, sizeof(float) * (size_t)g_fNumCells);
    const int n = g_fNumY;
    const float cp = (f->density * ff_maxf(f->h, 1e-6f)) / ff_maxf(dt, 1e-6f);
    for (int iter = 0; iter < numIters; iter++) {
        for (int i = 1; i < f->fNumX - 1; i++) {
            for (int j = 1; j < f->fNumY - 1; j++) {
                int center = i * n + j;
                if (g_cellType[center] != FLUID_CELL) continue;
                int left = (i - 1) * n + j;
                int right = (i + 1) * n + j;
                int bottom = i * n + j - 1;
                int top = i * n + j + 1;
                float sx0 = f->s[left];
                float sx1 = f->s[right];
                float sy0 = f->s[bottom];
                float sy1 = f->s[top];
                float ssum = sx0 + sx1 + sy0 + sy1;
                if (ssum == 0.0f) continue;
                float div = (g_u[right] - g_u[center]) + (g_v[top] - g_v[center]);
                if (compensateDrift) {
                    float compression = g_particleDensity[center] - g_particleRestDensity;
                    if (compression > 0.0f) div -= 1.0f * compression;
                }
                float p = (-div / ssum) * overRelaxation;
                g_p[center] += cp * p;
                g_u[center] -= sx0 * p;
                g_u[right]  += sx1 * p;
                g_v[center] -= sy0 * p;
                g_v[top]    += sy1 * p;
            }
        }
    }
}

// Map particles to a fixed 10x10 display grid; 1.0 where any fluid exists.
static void update_cell_colors_from_types(const FlipFluid *f, float *outColors) {
    if (!f || !outColors || !g_cellType) return;
    // Export in ROW-MAJOR layout (row*10 + col) to match JS getMiddle64Colors
    const int n = g_fNumY;
    for (int k = 0; k < GRID_X * GRID_Y; k++) outColors[k] = 0.0f;
    for (int x = 0; x < f->fNumX && x < GRID_X; x++) {
        for (int y = 0; y < f->fNumY && y < GRID_Y; y++) {
            int src = x * n + y;           // solver index
            int dst = y * GRID_X + x;      // row-major index (row=y, col=x)
            if (g_cellType[src] == FLUID_CELL) outColors[dst] = 1.0f;
        }
    }
}

// This implements a more complete FLIP-inspired step: integrate, separate,
// transfers P->G, pressure solve, transfers G->P, and color update.
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
    ensure_grid_alloc(f);
    ensure_alloc_particle_vel(f);

    // 1) Integrate particles with gravity
    integrate_particles(f, dt, gravity_x, gravity_y);

    // 2) Optionally push overlapping particles apart
    if (separateParticles && numParticleIters > 0) {
        push_particles_apart(f, numParticleIters);
    }

    // 3) Collide with tank boundaries
    handle_collisions(f);

    // 4) P->G, density, pressure, G->P
    transfer_velocities(f, 1, 0.0f);
    update_particle_density(f);
    solve_incompressibility(f, numPressureIters, dt, overRelaxation, compensateDrift);
    transfer_velocities(f, 0, ff_clamp(flipRatio, 0.0f, 1.0f));

    // 5) Colors from grid types
    static float cellColor[GRID_X * GRID_Y];
    update_cell_colors_from_types(f, cellColor);

    if (f && f->cellColor) {
        // Copy to the provided buffer. Caller allocated this; we copy up to 10x10.
        for (int i = 0; i < GRID_X * GRID_Y; i++) f->cellColor[i] = cellColor[i];
    }
}
