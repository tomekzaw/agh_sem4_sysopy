CC=gcc
CFLAGS=-Wall$(if $(O), -O$(O))

static: ../zad1/libblocks.a ../zad2/test.c
	$(CC) -c $(CFLAGS) ../zad2/test.c
	$(CC) $(CFLAGS) -o test test.o -L../zad1 -lblocks

shared: ../zad1/libblocks.so ../zad2/test.c
	$(CC) -c $(CFLAGS) ../zad2/test.c
	$(CC) $(CFLAGS) -o test test.o -L../zad1/ -Wl,-rpath=../zad1/ -lblocks -ldl

dynamic: ../zad1/libblocks.so ../zad2/test.c
	$(CC) -c $(CFLAGS) ../zad2/test.c -D DYNAMIC
	$(CC) $(CFLAGS) -o test test.o -L../zad1/ -Wl,-rpath=../zad1/ -lblocks -ldl

../zad1/libblocks.a:
	make -C ../zad1 libblocks.a

../zad1/libblocks.so:
	make -C ../zad1 libblocks.so

clean:
	rm -f *.o test

raport3a:
	> raport3a.txt
	for library in static shared dynamic ; do \
		make $$library ; \
		echo "$$library\n" >> raport3a.txt ; \
		./test 1024 $$(cat ../zad2/args.txt) >> raport3a.txt ; \
    done

raport3b:
	> raport3b.txt
	for library in static shared dynamic ; do \
		for o in 1 2 3 ; do \
			make $$library O=$$o ; \
			echo "$$library -O$$o\n" >> raport3b.txt ; \
			./test 1024 $$(cat ../zad2/args.txt) >> raport3b.txt ; \
		done ; \
	done
