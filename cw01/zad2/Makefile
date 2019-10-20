CC=gcc
CFLAGS=-Wall

test: test.o ../zad1/libblocks.a
	$(CC) $(CFLAGS) -o test test.o -L../zad1/ -Wl,-rpath=../zad1/ -lblocks

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

../zad1/libblocks.a:
	make -C ../zad1 libblocks.a

clean:
	rm -f *.o test

raport2: test
	./test 1024 `cat args.txt` > raport2.txt

