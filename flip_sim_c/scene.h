#ifndef SCENE_H
#define SCENE_H

#include <stdbool.h>
#include "flip_fluid.h"
/*
 * Scene struct: represents your fluid simulation settings (converted from JS object).
 */
typedef struct Scene
{
    float gravity_x;        // Gravity in X direction
    float gravity_y;        // Gravity in Y direction
    float dt;               // Time step (seconds per frame)
    float flipRatio;        // FLIP/PIC blend ratio
    int   numPressureIters; // Iterations for pressure solver
    int   numParticleIters; // Iterations for particle correction
    unsigned long frameNr;  // Frame counter
    float overRelaxation;   // Over-relaxation factor for solver
    bool  compensateDrift;  // Whether to compensate drift
    bool  separateParticles;// Whether to separate overlapping particles
    float obstacleX;        // Obstacle X position
    float obstacleY;        // Obstacle Y position
    float obstacleRadius;   // Obstacle radius
    bool  paused;           // Pause flag
    bool  showObstacle;     // Show obstacle flag
    float obstacleVelX;     // Obstacle velocity in X
    float obstacleVelY;     // Obstacle velocity in Y
    bool  showParticles;    // Show particles flag
    bool  showGrid;         // Show grid flag
    FlipFluid  *fluid;            // Pointer to fluid simulation object (placeholder)
} Scene;

/*
 * Function: scene_default
 * -----------------------
 * Returns a Scene struct initialized with default values.
 *
 * Usage:
 *   Scene s = scene_default();
 */
Scene scene_default(void);

/*
 * Function: scene_init
 * --------------------
 * Initializes an existing Scene struct pointer with default values.
 *
 * Usage:
 *   Scene s;
 *   scene_init(&s);
 */
void scene_init(Scene *s);

#endif // SCENE_H
