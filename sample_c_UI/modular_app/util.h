#ifndef UTIL_H
#define UTIL_H

#include <windows.h>
#include <commctrl.h>  // Ensure this is included for trackbar support

#define SIZE 8
#define CELL_SIZE 50

extern int grid[SIZE][SIZE];
extern HWND hwndTrackbar, hwndStatic, hwndStartButton, hwndPauseButton, hwndStateTextbox;
extern BOOL isRunning;

void RandomizeGrid();
void DrawGrid(HDC hdc);
void UpdateStateTextbox(HWND hwnd);
void InitUI(HWND hwnd);
void UpdateTrackbarValue();

#endif
