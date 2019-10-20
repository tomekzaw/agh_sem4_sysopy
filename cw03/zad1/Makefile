CC=gcc
CFLAGS=-Wall -Wextra

main: main.o
	$(CC) $(CFLAGS) -o main main.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -DNFTW -DWAIT
	# -DNFTW uses nftw() instead of opendir(), stat(), closedir()
	# -DWAIT guarantees order in accordance with the directory tree

clean:
	rm -f *.o main
