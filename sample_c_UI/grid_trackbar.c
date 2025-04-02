#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

#define SIZE 8
#define CELL_SIZE 50
#define TIMER_ID 1
#define TIMER_INTERVAL 2000  // 2 seconds

// Global variables for the grid and trackbar
int grid[SIZE][SIZE] = {0};
HWND hwndTrackbar, hwndStatic;

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
            HBRUSH hBrush = CreateSolidBrush(grid[i][j] ? RGB(0, 0, 0) : RGB(255, 0, 0)); // Black or Red
            RECT rect = { j * CELL_SIZE, i * CELL_SIZE, (j + 1) * CELL_SIZE, (i + 1) * CELL_SIZE };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }
    }
}

// Function to initialize the trackbar
void InitTrackbar(HWND hwnd) {
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
}

// Function to update the trackbar value display
void UpdateTrackbarValue() {
    int value = SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0);
    char buffer[50];
    sprintf(buffer, "Trackbar Value: %d", value);
    SetWindowText(hwndStatic, buffer);
}

// Windows procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            srand((unsigned int)time(NULL));  // Seed random
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);  // Set up the timer
            InitTrackbar(hwnd);  // Initialize trackbar
            break;

        case WM_TIMER:
            RandomizeGrid();  // Update grid
            InvalidateRect(hwnd, NULL, TRUE);  // Force redraw
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawGrid(hdc);  // Draw the grid
            EndPaint(hwnd, &ps);
        } break;

        case WM_HSCROLL:
            if ((HWND)lParam == hwndTrackbar) {
                UpdateTrackbarValue();  // Update value text when slider is moved
            }
            break;

        case WM_CLOSE:
            KillTimer(hwnd, TIMER_ID);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "GridTrackbarWindow";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("GridTrackbarWindow", "Grid & Trackbar", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, SIZE * CELL_SIZE + 50, SIZE * CELL_SIZE + 150,
                             NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
