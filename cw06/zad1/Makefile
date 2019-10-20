CC=gcc
CFLAGS=-Wall -Wextra

all: server client

server: server.o
	$(CC) $(CFLAGS) -o server server.o

server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c

client: client.o
	$(CC) $(CFLAGS) -o client client.o

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o server client
