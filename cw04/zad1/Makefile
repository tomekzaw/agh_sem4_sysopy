CC=gcc
CFLAGS=-Wall -Wextra

all: main_v1 main_v2

main_v1: main_v1.o
	$(CC) $(CFLAGS) -o main_v1 main_v1.o

main_v1.o: main_v1.c
	$(CC) $(CFLAGS) -c main_v1.c -DCTIME -DPAUSE

# Date and time format:
#	-DCTIME => "Thu Apr  4 16:58:15 2019"
#	-DSTRFTIME => "2019-04-04 16:58:15"

# Waiting for signal:
#	-DPAUSE => pause(2)
#	-DSIGSUSPEND => sigsuspend(2)

# Debug utilities:
#	-DDEBUG => usleep(1e5);

main_v2: main_v2.o
	$(CC) $(CFLAGS) -o main_v2 main_v2.o

main_v2.o: main_v2.c
	$(CC) $(CFLAGS) -c main_v2.c -DSIGSUSPEND

# Waiting for signal:
#	-DPAUSE => pause(2)
#	-DSIGSUSPEND => sigsuspend(2)

clean:
	rm -f *.o main_v1 main_v2
