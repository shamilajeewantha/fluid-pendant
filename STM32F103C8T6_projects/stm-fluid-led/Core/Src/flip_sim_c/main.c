#ifdef _WIN32
#include <windows.h> // Includes the Windows API functions, data types, and macros.
#include <commctrl.h> //Provides support for common controls like trackbars.
#include <stdlib.h>
#include <time.h>
#include "util.h"
#include <stdio.h>

// The #pragma directive is a preprocessor instruction used to provide specific instructions to the compiler.
// Oterwise, Command-line Compilation → Add -lcomctl32 when using a compiler like MinGW.
// like this : gcc grid_trackerbar_start.c util.c -o grid_trackerbar_start.exe -mwindows -lcomctl32
// only works with MSVC (Microsoft Visual C++ Compiler) and does not work with GCC (MinGW).
// #pragma comment(lib, "comctl32.lib")

#define TIMER_ID 1
#define TIMER_INTERVAL 20  // 2 seconds

// Windows procedure function
// This function handles various window messages (WM_* messages).
// HWND hwnd : Unique identifier for the window receiving the message.
// WPARAM wParam : Typically holds additional message-specific data (e.g., key codes for WM_KEYDOWN).
// LPARAM lParam : Typically holds additional message-specific data (e.g., mouse coordinates for WM_MOUSEMOVE).
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // time(NULL) : Gets the current time in seconds since January 1, 1970 (Unix epoch).
            // Converts the time value to an unsigned integer (to match srand’s expected input).
            // srand(value) : Seeds the random number generator with the current time.
            // If you do not call srand(), rand() will generate the same sequence of numbers every time you run the program.
            srand((unsigned int)time(NULL));  // Seed random
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);  // It starts a timer (SetTimer()), which triggers WM_TIMER every 2 seconds.
            InitUI(hwnd);  // From utils.c. Initialize UI elements
            break;

        case WM_TIMER:
            if (!scene.paused) { // isRunning is a global variable defined in util.h
                // RandomizeGrid();  // From utils.c. Update grid
                SimulateFluid(&scene);  // From utils.c. Update grid
                InvalidateRect(hwnd, NULL, TRUE);  // Force redraw
            }
            break;

        // When Windows requests a redraw (e.g., resizing or InvalidateRect() was called):
        case WM_PAINT: {
            PAINTSTRUCT ps; // a structure that Windows populates to track painting details
            HDC hdc = BeginPaint(hwnd, &ps); // Retrieves a device context (HDC) for the window. It's an abstraction of the actual graphics hardware used to draw things on the screen.
            DrawGrid(hdc);  // From utils.c. Draw the grid
            EndPaint(hwnd, &ps);
        } break;

        // a Windows message that is sent when the user interacts with a horizontal scrollbar or trackbar (a slider, in this case).
        case WM_HSCROLL:
            if ((HWND)lParam == hwndTrackbar) { // hwndTrackbar is exported as global var in util.h
                UpdateTrackbarValue();  // Update value text when slider is moved
            }
            break;

        // a Windows message that is sent when a user interacts with a control (like a button, menu item, or other interactive UI element) in a window.
        case WM_COMMAND:
            switch (LOWORD(wParam)) { // gives us the ID of the control that sent the message. The ID can be used to figure out which button was clicked.
                case 2:  // Start button
                    SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL);
                    StartSimulation(hwnd);
                    break;
                case 3:  // Pause button
                    KillTimer(hwnd, TIMER_ID);
                    PauseSimulation(hwnd);
                    break;
            }
            break;

        // a Windows message that is sent when the user attempts to close the window (e.g., by clicking the close button in the window's title bar).
        case WM_CLOSE:
            KillTimer(hwnd, TIMER_ID);
            PostQuitMessage(0); // a message to the message queue indicating that the application should terminate. This will cause the main message loop (GetMessage()) to exit and the program to end.
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam); // a Windows function that is called to handle any messages that are not explicitly handled by your WindowProc function.
            // this would handle things like WM_SIZE (window resizing) or WM_KEYDOWN (key presses).
    }
    return 0;
}

// Entry point
// In a typical C program, this entry point is the main() function. However, in Windows GUI applications, the entry point is often WinMain().
// hInstance : This is the handle of the current instance of the application. It's provided by Windows when the program is executed, and it represents the application's instance in memory.
// LPSTR lpCmdLine : This is the command line passed to the program when it is executed. It contains the command-line arguments used to launch the application
// int nCmdShow : This argument tells the application how to display the window when it first appears. It can have values like SW_SHOW (to show the window), SW_HIDE (to hide the window), SW_MINIMIZE, SW_MAXIMIZE, etc
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Allocate console to see printf output
    AllocConsole();
    freopen("CONOUT$", "w", stdout); // Redirect stdout to the console
    printf("Program started!\n"); // Prints to the newly opened console

    // This initializes a structure (INITCOMMONCONTROLSEX) that tells Windows which common controls to initialize.
    // ICC_BAR_CLASSES is a flag that specifies which specific classes of controls to initialize (in this case, bar classes, which include trackbars and toolbars).
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    // Register the Window Class
    WNDCLASS wc = {0}; // The {0} syntax sets all the members of the structure to zero, ensuring no garbage values.
    wc.lpfnWndProc = WindowProc; //  Whenever Windows needs to interact with the window, it will call this function to process the message.
    wc.hInstance = hInstance;  // This tells Windows which instance of the program this window belongs to.
    wc.lpszClassName = "GridTrackbarWindow"; // This sets the class name for the window. It can be used to identify windows of this type later.
    RegisterClass(&wc);  // This tells the operating system that this class exists and will be used to create windows.

    // This function creates a window based on the class we registered earlier
    // "Grid & Trackbar with Controls": The window title that appears in the window’s title bar.
    // WS_OVERLAPPEDWINDOW: This is the style of the window. WS_OVERLAPPEDWINDOW is a predefined constant that creates a standard window with a title bar, menu, and borders.
    // CW_USEDEFAULT: This specifies the x-coordinate and y-coordinate of the window’s position. CW_USEDEFAULT means the window is placed in a default position.
    // SIZE * CELL_SIZE + 200: The width of the window, calculated based on the grid size and the cell size, plus some additional space (200 pixels) for UI controls.
    // SIZE * CELL_SIZE + 200: The height of the window, similarly calculated.
    HWND hwnd = CreateWindow("GridTrackbarWindow", "Grid & Trackbar with Controls", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, SIZE * CELL_SIZE + 200, SIZE * CELL_SIZE + 200,
                             NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);     // This function tells Windows to show the window.                         
    UpdateWindow(hwnd); // This function forces the window to repaint itself if necessary.
    // It ensures that the window content is updated right after being shown, so you don't see a blank window before the content is drawn.
    
    // Message Loop
    MSG msg = {0}; // This initializes a MSG structure, which is used to store message information. The {0} syntax sets all fields of the MSG structure to 0.
    // The while loop continues running until GetMessage returns FALSE, which happens when the user closes the window (generating a WM_QUIT message).
    while (GetMessage(&msg, NULL, 0, 0)) { // &msg: A pointer to the MSG structure where the message will be stored.
        TranslateMessage(&msg); // This translates keyboard messages (e.g., converting WM_KEYDOWN to WM_CHAR) so that they can be processed by the window's message handler.
        DispatchMessage(&msg); // This sends the message to the window procedure (WindowProc), where the message is processed.
    }
    printf("Program ended!");

    return 0;
}
#endif
