#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <signal.h>
#include <linux/msg.h>

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

#define MSG_MAX_LENGTH MSGMAX //<= 4056 (linux/msg.h)
#define SERVER_MSG_QUEUE_NAME "/tzawadzk_server"
#define CLIENT_MSG_QUEUE_NAME_PREFIX "/tzawadzk_client"
#define CLIENT_CMD_PROMPT_CHAR '>'
