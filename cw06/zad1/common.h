#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MSG_STOP 10
#define MSG_LIST 9
#define MSG_FRIENDS 8
#define MSG_INIT 7
#define MSG_ECHO 6
#define MSG_ADD 5
#define MSG_DEL 4
#define MSG_2ALL 3
#define MSG_2FRIENDS 2
#define MSG_2ONE 1

#define PROJ_ID 255 // must be nonzero
#define SERVER_QUEUE_KEY() ftok(getenv("HOME"), PROJ_ID) // $HOME
#define MSG_MAX_LENGTH 4096

/* message buffer for msgsnd and msgrcv calls */
struct msgbuf {
    long mtype;
    char mtext[MSG_MAX_LENGTH]; // just for convenience
};

