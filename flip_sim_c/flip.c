#include <stdio.h>

#define FLUID_CELL 0
#define AIR_CELL 1
#define SOLID_CELL 2

int clamp(int x, int min, int max) {
    if (x < min) return min;
    else if (x > max) return max;
    else return x;
}