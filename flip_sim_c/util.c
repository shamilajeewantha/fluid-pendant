#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

int grid[SIZE][SIZE] = {0};
HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
BOOL isRunning = TRUE;

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
            HBRUSH hBrush = CreateSolidBrush(grid[i][j] ? RGB(0, 0, 0) : RGB(0, 255, 0)); // Green or Red
            RECT rect = { j * CELL_SIZE, i * CELL_SIZE, (j + 1) * CELL_SIZE, (i + 1) * CELL_SIZE };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }
    }
}

// Function to update the state textbox
void UpdateStateTextbox(HWND hwnd) {
    SetWindowText(hwndStateTextbox, isRunning ? "Running" : "Paused");
}

// Function to update the trackbar value display
void UpdateTrackbarValue() {
    int value = SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0);
    char buffer[50];
    sprintf(buffer, "Trackbar Value: %d", value);
    SetWindowText(hwndStatic, buffer);
}

// Function to initialize UI elements
void InitUI(HWND hwnd) {
    hwndStatic = CreateWindowEx(0, "STATIC", "Trackbar Value: 0", 
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        50, SIZE * CELL_SIZE + 20, 300, 30,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    hwndTrackbar = CreateWindowEx(0, TRACKBAR_CLASS, "Trackbar", 
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
        50, SIZE * CELL_SIZE + 50, 300, 50,
        hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

    SendMessage(hwndTrackbar, TBM_SETRANGEMIN, TRUE, -90);
    SendMessage(hwndTrackbar, TBM_SETRANGEMAX, TRUE, 90);
    SendMessage(hwndTrackbar, TBM_SETPOS, TRUE, 0);
    SendMessage(hwndTrackbar, TBM_SETTICFREQ, 18, 0);

    hwndStartButton = CreateWindow("BUTTON", "Start", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, SIZE * CELL_SIZE + 20, 100, 30, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

    hwndPauseButton = CreateWindow("BUTTON", "Pause", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   400, SIZE * CELL_SIZE + 60, 100, 30, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

    hwndStateTextbox = CreateWindow("EDIT", "Running", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                                    400, SIZE * CELL_SIZE + 100, 100, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
}
