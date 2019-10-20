CC=gcc
CFLAGS=-Wall

static: libblocks.a

shared: libblocks.so

blocks.o: blocks.c blocks.h
	$(CC) -c -fPIC $(CFLAGS) blocks.c

libblocks.a: blocks.o
	ar rcs libblocks.a blocks.o

libblocks.so: blocks.o
	$(CC) -shared -fPIC $(CFLAGS) -o libblocks.so blocks.o

clean:
	rm -f *.o *.so *.a
