CC = gcc
CFLAGS = -std=c99 -O3 -pedantic -Wall  \
		 -lm `pkg-config --cflags --libs sdl2`

CFILES = main.c

ttysdl: $(CFILES) clean
	$(CC) $(CFLAGS) -o ttysdl -pthread -O3 -DNDEBUG $(CFILES)

tcc: $(CFILES) clean
	tcc -o ttysdl -D_REENTRANT -lpthread $(CFILES)

debug: $(CFILES) clean
	$(CC) $(CFLAGS) -o ttysdl -pthread -g -DDEBUG $(CFILES)

prof: $(CFILES) clean
	$(CC) $(CFLAGS) -o ttysdl -pthread -pg $(CFILES)

clean:
	rm -f ttysdl
