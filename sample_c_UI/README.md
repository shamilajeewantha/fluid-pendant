gcc windows_showgrid.c -o windows_showgrid.exe -lgdi32
./windows_showgrid.exe

gcc windows_slider.c -o windows_slider.exe -lcomctl32
./windows_slider.exe

gcc grid_trackbar.c -o grid_trackbar.exe -mwindows -lcomctl32
./grid_trackbar.exe

gcc grid_trackerbar_start.c -o grid_trackerbar_start.exe -mwindows -lcomctl32
./grid_trackerbar_start.exe

gcc grid_trackerbar_start.c util.c -o grid_trackerbar_start.exe -mwindows -lcomctl32
./grid_trackerbar_start.exe
