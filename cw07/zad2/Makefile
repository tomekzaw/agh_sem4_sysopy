CC=gcc
CFLAGS=-Wall -Wextra # -DSLOW_MOTION

all: loader trucker parent

loader: loader.o common.o
	$(CC) $(CFLAGS) -o loader loader.o common.o -lrt -lpthread

loader.o: loader.c common.h
	$(CC) $(CFLAGS) -c loader.c

trucker: trucker.o common.o
	$(CC) $(CFLAGS) -o trucker trucker.o common.o -lrt -lpthread

trucker.o: trucker.c common.h
	$(CC) $(CFLAGS) -c trucker.c
	
parent: parent.o common.o
	$(CC) $(CFLAGS) -o parent parent.o common.o

parent.o: parent.c common.h
	$(CC) $(CFLAGS) -c parent.c

common.o: common.h common.c
	$(CC) $(CFLAGS) -c common.c

clean:
	rm -f *.o loader trucker parent
