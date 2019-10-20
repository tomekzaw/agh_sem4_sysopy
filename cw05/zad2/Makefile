CC=gcc
CFLAGS=-Wall -Wextra

all: master slave

master: master.o
	$(CC) $(CFLAGS) -o master master.o

master.o: master.c
	$(CC) $(CFLAGS) -c master.c

slave: slave.o
	$(CC) $(CFLAGS) -o slave slave.o

slave.o: slave.c
	$(CC) $(CFLAGS) -c slave.c

clean:
	rm -f *.o master slave
