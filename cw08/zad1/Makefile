CC=gcc
CFLAGS=-Wall -Wextra #-funroll-loops

all: main

main: main.o
	$(CC) $(CFLAGS) -o main main.o -lm -pthread

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o main

measurements:
	sh Times.sh
