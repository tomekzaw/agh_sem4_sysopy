CC=gcc
CFLAGS=-Wall -Wextra #-DCONFIRM
# without -DCONFIRM => a) send sequence of signals and then wait for response
# with -DCONFIRM => b) send signals one by one and wait for confirmation every time

all: sender catcher

sender: sender.o common.o
	$(CC) $(CFLAGS) -o sender sender.o common.o

sender.o: sender.c common.h
	$(CC) $(CFLAGS) -c sender.c

catcher: catcher.o common.o
	$(CC) $(CFLAGS) -o catcher catcher.o common.o

catcher.o: catcher.c common.h
	$(CC) $(CFLAGS) -c catcher.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

clean:
	rm -f *.o sender catcher
