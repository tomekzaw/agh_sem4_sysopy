CC=gcc
CFLAGS=-Wall -Wextra

monitor: monitor.o watch tester
	$(CC) $(CFLAGS) -o monitor monitor.o

monitor.o: monitor.c
	$(CC) $(CFLAGS) -c monitor.c

watch: watch.o
	$(CC) $(CFLAGS) -o watch watch.o

watch.o: watch.c
	$(CC) $(CFLAGS) -c watch.c

tester: tester.o
	$(CC) $(CFLAGS) -o tester tester.o

tester.o: tester.c
	$(CC) $(CFLAGS) -c tester.c

clean:
	rm -f *.o monitor watch tester archiwum/*
