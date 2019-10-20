CC=gcc
CFLAGS=-Wall -Wextra

all: main

main: main.o
	$(CC) $(CFLAGS) -o main main.o -pthread

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

test: all
	./main 6 2 3 5

clean:
	rm -f *.o main
