CC=gcc
CFLAGS=-Wall

main: main.o
	$(CC) $(CFLAGS) -o main main.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o main

measurement: main
	> wyniki.txt
	for nbytes in 1 4 512 1024 4096 8192 ; do \
		for nrecords in 1024 2048 ; do \
			echo "nrecords=$$nrecords nbytes=$$nbytes" >> wyniki.txt ; \
			./main generate tmp.txt $$nrecords $$nbytes ; \
			echo -e "\ncopy_sys" >> wyniki.txt ; \
			{ /usr/bin/time -p ./main copy tmp.txt tmp_sys.txt $$nrecords $$nbytes sys ; } 2>> wyniki.txt ; \
			echo -e "\ncopy_lib" >> wyniki.txt ; \
			{ /usr/bin/time -p ./main copy tmp.txt tmp_lib.txt $$nrecords $$nbytes lib ; } 2>> wyniki.txt ; \
			echo -e "\nsort_sys" >> wyniki.txt ; \
			{ /usr/bin/time -p ./main sort tmp_sys.txt $$nrecords $$nbytes sys ; } 2>> wyniki.txt ; \
			echo -e "\nsort_lib" >> wyniki.txt ; \
			{ /usr/bin/time -p ./main sort tmp_lib.txt $$nrecords $$nbytes lib ; } 2>> wyniki.txt ; \
			echo -e "\n" >> wyniki.txt ; \
			rm -f tmp*.txt ; \
		done ; \
	done