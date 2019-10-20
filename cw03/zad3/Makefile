CC=gcc
CFLAGS=-Wall -Wextra

monitor: monitor.o watch
	$(CC) $(CFLAGS) -o monitor monitor.o

monitor.o: ../zad2/monitor.c
	$(CC) $(CFLAGS) -c ../zad2/monitor.c -DZAD3

watch: watch.o
	$(CC) $(CFLAGS) -o watch watch.o

watch.o: ../zad2/watch.c
	$(CC) $(CFLAGS) -c ../zad2/watch.c -DZAD3

clean:
	rm -f *.o monitor watch archiwum/*
