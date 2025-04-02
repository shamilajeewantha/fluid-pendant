#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 8
#define CELL_SIZE 50
#define TIMER_ID 1
#define TIMER_INTERVAL 2000  // 500ms = 0.5 seconds

int grid[SIZE][SIZE] = {0};

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
            HBRUSH hBrush = CreateSolidBrush(grid[i][j] ? RGB(0, 0, 0) : RGB(255, 0, 0));  // Black or Red
            RECT rect = { j * CELL_SIZE, i * CELL_SIZE, (j + 1) * CELL_SIZE, (i + 1) * CELL_SIZE };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }
    }
}

// Windows procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            srand((unsigned int)time(NULL));  // Seed random
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);  // Set up a timer
            break;

        case WM_TIMER:
            RandomizeGrid();  // Update grid
            InvalidateRect(hwnd, NULL, TRUE);  // Force redraw
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawGrid(hdc);
            EndPaint(hwnd, &ps);
        } break;

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
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "GridWindowClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("GridWindowClass", "8x8 Grid", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, SIZE * CELL_SIZE + 16, SIZE * CELL_SIZE + 39,
                             NULL, NULL, hInstance, NULL);
    
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
