#include <stdio.h>
#include <math.h>  // Include the math library for sin and cos

#define WIDTH 800
#define HEIGHT 600

float velocity[WIDTH][HEIGHT];

void update_simulation() {
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            velocity[i][j] = (float)sin(i * 0.1) * (float)cos(j * 0.1);
        }
    }
}

float get_velocity(int x, int y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return velocity[x][y];
    }
    return 0.0f;
}
