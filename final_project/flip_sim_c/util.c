#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <commctrl.h>
#include "scene.h"
#include "flip_utils.h"
#include "flip_fluid.h"
#include <math.h>

#define DEG2RAD(angle) ((angle) * M_PI / 180.0)

// Global Variables
int grid[GRID_Y][GRID_X] = {0};
HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
Scene scene;

// Function to randomize the grid
void RandomizeGrid() {
    for (int i = 0; i < GRID_Y; i++) {
        for (int j = 0; j < GRID_X; j++) {
            grid[i][j] = rand() % 2;  // Random 0 or 1
        }
    }
}


void UpdateGridFromFluid(Scene* scene) {
    if (!scene->fluid || !scene->fluid->cellColor) return;

    // cellColor is already cropped to exactly GRID_X x GRID_Y by
    // update_cell_colors_from_types() in flip_fluid.c (the solver's own internal grid is
    // padded larger than this — research.md Decision 7 — but that padding is invisible
    // here). "Up" in the tank (high simulation row, the open top) is drawn at the top of
    // the screen, so display row i maps to solver row (GRID_Y - 1 - i); columns map directly.
    // grid[][] holds a 0-255 brightness level (research.md Decision 13) rather than a
    // binary flag — cellColor is already a continuous 0.0-1.0 value, just scaled for the
    // RGB() green channel DrawGrid uses below.
    for (int i = 0; i < GRID_Y; i++) {
        int simRow = GRID_Y - 1 - i;
        for (int j = 0; j < GRID_X; j++) {
            float g = scene->fluid->cellColor[simRow * GRID_X + j];
            grid[i][j] = (int)(g * 255.0f);
        }
    }
}


void SimulateFluid(Scene* scene) {
    if (!scene->paused && scene->fluid) {
        simulateFlipFluid(
            scene->fluid,
            scene->dt,
            scene->gravity_x,
            scene->gravity_y,
            scene->flipRatio,
            scene->numPressureIters,
            scene->numParticleIters,
            scene->overRelaxation,
            scene->compensateDrift,
            scene->separateParticles
        );
        scene->frameNr++;

        UpdateGridFromFluid(scene);  // Update grid based on fluid colors
    }
}




// Function to draw the grid
// Draws row indices (1..GRID_Y) in the left margin and column indices
// (1..GRID_X) in the top margin, aligned with DrawGrid's cell positions.
// Labels are 1-based for display only; the underlying grid[][] array stays 0-indexed.
static void DrawGridLabels(HDC hdc) {
    char buf[8];
    SetBkMode(hdc, TRANSPARENT);
    for (int j = 0; j < GRID_X; j++) {
        wsprintf(buf, "%d", j + 1);
        RECT r = { GRID_LEFT + j * CELL_SIZE, 0, GRID_LEFT + (j + 1) * CELL_SIZE, GRID_TOP };
        DrawText(hdc, buf, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < GRID_Y; i++) {
        wsprintf(buf, "%d", i + 1);
        RECT r = { 0, GRID_TOP + i * CELL_SIZE, GRID_LEFT, GRID_TOP + (i + 1) * CELL_SIZE };
        DrawText(hdc, buf, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void DrawGrid(HDC hdc) {
    // A dark-gray pen draws a 1px border around each cell (via Rectangle, which fills
    // with the current brush and outlines with the current pen in one call), so
    // adjacent cells are visibly separated.
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    for (int i = 0; i < GRID_Y; i++) {
        for (int j = 0; j < GRID_X; j++) {
            HBRUSH hBrush = CreateSolidBrush(RGB(0, grid[i][j], 0)); // grid[i][j] is a 0-255 brightness level (research.md Decision 13).
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
            // GRID_LEFT/GRID_TOP shift the grid right/down to leave room for DrawGridLabels' margins.
            RECT rect = { GRID_LEFT + j * CELL_SIZE, GRID_TOP + i * CELL_SIZE,
                          GRID_LEFT + (j + 1) * CELL_SIZE, GRID_TOP + (i + 1) * CELL_SIZE };  // This defines the boundaries of a rectangle. Each cell in the grid is drawn as a rectangle with a size defined by CELL_SIZE.
            Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hBrush);
        }
    }
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    DrawGridLabels(hdc);
}

// Function to update the state textbox
void UpdateStateTextbox() {
    SetWindowText(hwndStateTextbox, scene.paused ? "Paused" : "Running"); //  This function updates the state of a textbox (hwndStateTextbox) to display whether the simulation is running or paused.
}

void StartSimulation(HWND hwnd) {
    scene.paused = FALSE;
    UpdateStateTextbox();
    printf("Simulation Started\n");
}

void PauseSimulation(HWND hwnd) {
    scene.paused = TRUE;
    UpdateStateTextbox();
    printf("Simulation Paused\n");
}

// Function to update the trackbar value display
void UpdateTrackbarValue() {
    int angle = SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0); // Get trackbar position (-90 to 90)
    char buffer[50];
    sprintf(buffer, "Angle: %d°", angle);
    SetWindowText(hwndStatic, buffer);

    // Convert angle to radians and update scene gravity.
    // Angle 0 => gravity down (negative Y). Positive angle tilts clockwise.
    double radians = DEG2RAD(angle);
    scene.gravity_x =  sin(radians) * 9.81;  // right is positive X
    scene.gravity_y = -cos(radians) * 9.81;  // up is positive Y (so down is negative)

    printf("Gravity updated: (%.2f, %.2f)\n", scene.gravity_x, scene.gravity_y);
}


void InitFlip(){ // Declaration of InitFlip function
    scene = scene_default(); // Initialize global scene

    /* 2. GET attributes (reading values) */
    printf("Gravity: (%.2f, %.2f)\n", scene.gravity_x, scene.gravity_y);

    setupScene(&scene);

    printf("First particle position: (%f, %f)\n",
           scene.fluid->particlePos[0],
           scene.fluid->particlePos[1]);

}


// Function to initialize UI elements
// This function initializes the user interface by creating various controls, such as static text, buttons, and a trackbar (slider).
void InitUI(HWND hwnd) {
    // CreateWindowEx: This function creates a new window (or control), in this case, a static text control to display the trackbar value.
    hwndStatic = CreateWindowEx(0, "STATIC", "Trackbar Value: 0", 
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        50, GRID_TOP + GRID_Y * CELL_SIZE + 20, 300, 30,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    // TBS_AUTOTICKS | TBS_ENABLESELRANGE: These styles specify that the trackbar will automatically display tick marks and support a selectable range.
    hwndTrackbar = CreateWindowEx(0, TRACKBAR_CLASS, "Trackbar", 
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
        50, GRID_TOP + GRID_Y * CELL_SIZE + 50, 300, 50,
        hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

    SendMessage(hwndTrackbar, TBM_SETRANGEMIN, TRUE, -90); // Set the minimum and maximum values of the trackbar (-90 to 90).
    SendMessage(hwndTrackbar, TBM_SETRANGEMAX, TRUE, 90);
    SendMessage(hwndTrackbar, TBM_SETPOS, TRUE, 0); // Set the initial position of the trackbar to 0.
    SendMessage(hwndTrackbar, TBM_SETTICFREQ, 18, 0); // Set the frequency of tick marks (every 18 units).

    // The button’s control ID is 2 (used to identify it later in the WindowProc function).
    hwndStartButton = CreateWindow("BUTTON", "Start", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, GRID_TOP + GRID_Y * CELL_SIZE + 20, 100, 30, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

    hwndPauseButton = CreateWindow("BUTTON", "Pause", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, GRID_TOP + GRID_Y * CELL_SIZE + 60, 100, 30, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

    hwndStateTextbox = CreateWindow("EDIT", "Not Started", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                                    400, GRID_TOP + GRID_Y * CELL_SIZE + 100, 100, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);


    printf("UI Initialized\n");
    printf("Initializing flip....");
    InitFlip();
    printf("Flip Initialized.\n");


}
