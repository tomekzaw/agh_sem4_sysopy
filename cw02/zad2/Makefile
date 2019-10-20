CC=gcc
CFLAGS=-Wall

stat: main.c
	$(CC) $(CFLAGS) -c main.c
	$(CC) $(CFLAGS) -o main main.o

nftw: main.c
	$(CC) $(CFLAGS) -c main.c -DNFTW
	$(CC) $(CFLAGS) -o main main.o

clean:
	rm -f *.o main
