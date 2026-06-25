CC=gcc
CFLAGS=-mwindows -lcomctl32
SRC=$(wildcard *.c)
EXE=main.exe

all:
	$(CC) $(SRC) -o $(EXE) $(CFLAGS)

clean:
	rm -f $(EXE)
