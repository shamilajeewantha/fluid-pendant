#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <time.h>
#include "util.h"

#pragma comment(lib, "comctl32.lib")

#define TIMER_ID 1
#define TIMER_INTERVAL 2000  // 2 seconds

// Windows procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            srand((unsigned int)time(NULL));  // Seed random
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);  // Set up the timer
            InitUI(hwnd);  // Initialize UI elements
            break;

        case WM_TIMER:
            if (isRunning) {
                RandomizeGrid();  // Update grid
                InvalidateRect(hwnd, NULL, TRUE);  // Force redraw
            }
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

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 2:  // Start button
                    isRunning = TRUE;
                    SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);
                    UpdateStateTextbox(hwnd);
                    break;
                case 3:  // Pause button
                    isRunning = FALSE;
                    KillTimer(hwnd, TIMER_ID);
                    UpdateStateTextbox(hwnd);
                    break;
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

    HWND hwnd = CreateWindow("GridTrackbarWindow", "Grid & Trackbar with Controls", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, SIZE * CELL_SIZE + 200, SIZE * CELL_SIZE + 200,
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
