#include "scene.h"
#include <stddef.h>  // for NULL

Scene scene_default(void)
{
    Scene s;
    s.gravity_x         = 0.0f;
    s.gravity_y         = -9.81f;
    s.dt                = 1.0f / 120.0f;   // ~120 FPS
    s.flipRatio         = 0.9f;
    s.numPressureIters  = 100;
    s.numParticleIters  = 2;
    s.frameNr           = 0UL;
    s.overRelaxation    = 1.9f;
    s.compensateDrift   = true;
    s.separateParticles = true;
    s.paused            = true;
    s.showParticles     = true;
    s.showGrid          = true;
    s.fluid             = NULL;
    return s;
}

void scene_init(Scene *s)
{
    if (!s) return;
    *s = scene_default();
}



/// Usage ////
// #include <stdio.h>
// #include "scene.h"
 
// int main(void)
// {
//     /* 1. Create a new scene with default values */
//     Scene scene = scene_default();

//     /* 2. GET attributes (reading values) */
//     printf("Gravity: (%.2f, %.2f)\n", scene.gravity_x, scene.gravity_y);
//     printf("Time step: %.6f\n", scene.dt);
//     printf("Obstacle radius: %.2f\n", scene.obstacleRadius);
//     printf("Paused? %s\n", scene.paused ? "Yes" : "No");

//     /* 3. SET attributes (modifying values) */
//     scene.paused = false;             // unpause
//     scene.obstacleX = 1.5f;           // move obstacle
//     scene.obstacleY = 2.0f;           // move obstacle
//     scene.frameNr += 1;               // advance frame counter
//     scene.gravity_y = -15.0f;         // make gravity stronger

//     /* 4. Show updated values */
//     printf("\n--- After Updates ---\n");
//     printf("Paused? %s\n", scene.paused ? "Yes" : "No");
//     printf("Obstacle position: (%.2f, %.2f)\n", scene.obstacleX, scene.obstacleY);
//     printf("Frame number: %lu\n", scene.frameNr);
//     printf("New gravity: (%.2f, %.2f)\n", scene.gravity_x, scene.gravity_y);

//     return 0;
// }
// End of usage //