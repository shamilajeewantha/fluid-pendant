gcc windows_showgrid.c -o windows_showgrid.exe -lgdi32

./windows_showgrid.exe

gcc windows_slider.c -o windows_slider.exe -lcomctl32
./windows_slider.exe


gcc grid_trackbar.c -o grid_trackbar.exe -mwindows -lcomctl32
./grid_trackbar.exe
