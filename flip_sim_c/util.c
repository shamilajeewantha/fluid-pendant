#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <commctrl.h>
#include "scene.h"
#include "flip_utils.h"

// Global Variables
int grid[SIZE][SIZE] = {0};
HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
Scene scene;

// Function to randomize the grid
void RandomizeGrid() {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            grid[i][j] = rand() % 2;  // Random 0 or 1
        }
    }
}

// Function to draw the grid
void DrawGrid(HDC hdc) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            HBRUSH hBrush = CreateSolidBrush(grid[i][j] ? RGB(0, 255, 0) : RGB(0, 0, 0)); // If grid[i][j] is 1, the brush color will be green (RGB(0, 255, 0)).
            RECT rect = { j * CELL_SIZE, i * CELL_SIZE, (j + 1) * CELL_SIZE, (i + 1) * CELL_SIZE };  // This defines the boundaries of a rectangle. Each cell in the grid is drawn as a rectangle with a size defined by CELL_SIZE.
            FillRect(hdc, &rect, hBrush); // This function fills the defined rectangle with the selected brush (hBrush), drawing it on the window.
            DeleteObject(hBrush);
        }
    }
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
    int value = SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0); // Sends a message to the trackbar to get its current position.
    char buffer[50];
    sprintf(buffer, "Trackbar Value: %d", value); // This formats the trackbar value into a string ("Trackbar Value: X" where X is the value from the trackbar).
    SetWindowText(hwndStatic, buffer);  // Sets the text of the static control (hwndStatic) to display the current trackbar value.
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
        50, SIZE * CELL_SIZE + 20, 300, 30,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    // TBS_AUTOTICKS | TBS_ENABLESELRANGE: These styles specify that the trackbar will automatically display tick marks and support a selectable range.
    hwndTrackbar = CreateWindowEx(0, TRACKBAR_CLASS, "Trackbar", 
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
        50, SIZE * CELL_SIZE + 50, 300, 50,
        hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

    SendMessage(hwndTrackbar, TBM_SETRANGEMIN, TRUE, -90); // Set the minimum and maximum values of the trackbar (-90 to 90).
    SendMessage(hwndTrackbar, TBM_SETRANGEMAX, TRUE, 90);
    SendMessage(hwndTrackbar, TBM_SETPOS, TRUE, 0); // Set the initial position of the trackbar to 0.
    SendMessage(hwndTrackbar, TBM_SETTICFREQ, 18, 0); // Set the frequency of tick marks (every 18 units).

    // The button’s control ID is 2 (used to identify it later in the WindowProc function).
    hwndStartButton = CreateWindow("BUTTON", "Start", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, SIZE * CELL_SIZE + 20, 100, 30, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

    hwndPauseButton = CreateWindow("BUTTON", "Pause", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, SIZE * CELL_SIZE + 60, 100, 30, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

    hwndStateTextbox = CreateWindow("EDIT", "Not Started", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                                    400, SIZE * CELL_SIZE + 100, 100, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);


    printf("UI Initialized\n");
    printf("Initializing flip....");
    InitFlip();
    printf("Flip Initialized.\n");


}
