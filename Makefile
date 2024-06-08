C = gcc
CFLAGS = -std=c99 -O3 -pedantic -Wall
LIBS = -lm `pkg-config --cflags --libs sdl2`

CFILES = main.c

ttysdl: $(CFILES) clean
	$(CC) $(CFLAGS) -o ttysdl -pthread -O3 -DNDEBUG $(CFILES) $(LIBS)

clean:
		rm -vf ttysdl
