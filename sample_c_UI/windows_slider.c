#include <windows.h>
#include <commctrl.h>  // For the trackbar control
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

// Global variables to store handles
HWND hwndTrackbar, hwndStatic;

// Function to initialize the trackbar and static text control
void InitTrackbar(HWND hwnd) {
    // Create static text control above the slider to display the value
    hwndStatic = CreateWindowEx(
        0, "STATIC", "Trackbar Value: 0",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        50, 20, 300, 30,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    // Create a trackbar (slider) with a range of -90 to 90
    hwndTrackbar = CreateWindowEx(
        0, TRACKBAR_CLASS, "Trackbar",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
        50, 50, 300, 50,
        hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

    // Set the range of the trackbar from -90 to 90
    SendMessage(hwndTrackbar, TBM_SETRANGEMIN, TRUE, -90);
    SendMessage(hwndTrackbar, TBM_SETRANGEMAX, TRUE, 90);
    SendMessage(hwndTrackbar, TBM_SETPOS, TRUE, 0);  // Default position (0)

    // Set tick marks on the trackbar for better visualization
    SendMessage(hwndTrackbar, TBM_SETTICFREQ, 18, 0);
}

// Function to update the text label with the current trackbar value
void UpdateTrackbarValue(HWND hwnd) {
    int value = SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0); // Get the current value
    char buffer[50];
    sprintf(buffer, "Trackbar Value: %d", value);

    // Update the text of the static control above the slider
    SetWindowText(hwndStatic, buffer);
}

// Window Procedure to handle messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            InitTrackbar(hwnd); // Initialize the trackbar and static text
            break;
        case WM_HSCROLL: {
            // This is triggered whenever the slider moves
            if ((HWND)lParam == hwndTrackbar) {
                UpdateTrackbarValue(hwnd);  // Update value text when slider is moved
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int main() {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const char *className = "TrackbarWindow";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;
    RegisterClass(&wc);

    // Create the window
    HWND hwnd = CreateWindowEx(
        0, className, "Trackbar Example",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    // Show the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
