#ifndef UTIL_H
#define UTIL_H
//  This is a standard inclusion guard mechanism that prevents multiple inclusions of the same header file in the same translation unit (source file). 
// If UTIL_H is not defined, the compiler will proceed to include the contents of the header file, and at the end, it defines UTIL_H to prevent any further inclusion of the file in the same compilation.
// This ensures that even if the header file is included multiple times (either directly or indirectly through other headers), its contents will only be processed once.s

#include <windows.h>
#include <commctrl.h>  // Ensure this is included for trackbar support
#include "scene.h"

#define SIZE 8
#define CELL_SIZE 50

extern int grid[SIZE][SIZE];
extern HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
extern Scene scene;

void RandomizeGrid();
void DrawGrid(HDC hdc);
void StartSimulation(HWND hwnd);
void PauseSimulation(HWND hwnd);
void InitUI(HWND hwnd);
void UpdateTrackbarValue();

#endif
// This is the end of the inclusion guard. The #endif marks the end of the #ifndef UTIL_H block, ensuring that the contents of the header file are only included once in any source file that includes util.h.