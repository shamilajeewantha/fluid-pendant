#ifndef UTIL_H
#define UTIL_H
//  This is a standard inclusion guard mechanism that prevents multiple inclusions of the same header file in the same translation unit (source file). 
// If UTIL_H is not defined, the compiler will proceed to include the contents of the header file, and at the end, it defines UTIL_H to prevent any further inclusion of the file in the same compilation.
// This ensures that even if the header file is included multiple times (either directly or indirectly through other headers), its contents will only be processed once.s

#include <windows.h>
#include <commctrl.h>  // Ensure this is included for trackbar support
#include "scene.h"     // also brings in flip_fluid.h's GRID_X (columns) / GRID_Y (rows)

// On-screen pixel size of one grid cell and the margin reserved for row/column
// index labels. Both are pure rendering constants — changing them cannot affect
// simulation behavior (research.md Decision 9); they have no relationship to
// tankWidth/tankHeight/h/r in flip_utils.c.
#define CELL_SIZE 30
#define LABEL_MARGIN 30
#define GRID_LEFT LABEL_MARGIN
#define GRID_TOP LABEL_MARGIN

extern int grid[GRID_Y][GRID_X];
extern HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
extern Scene scene;

void RandomizeGrid();
void DrawGrid(HDC hdc);
void StartSimulation(HWND hwnd);
void PauseSimulation(HWND hwnd);
void InitUI(HWND hwnd);
void UpdateTrackbarValue();
void SimulateFluid(Scene* scene);

#endif
// This is the end of the inclusion guard. The #endif marks the end of the #ifndef UTIL_H block, ensuring that the contents of the header file are only included once in any source file that includes util.h.