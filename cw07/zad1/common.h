#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define PROJ_ID 255 // must be non-zero
#define KEY() ftok(getenv("HOME"), PROJ_ID) // $HOME

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

struct package {
    unsigned int weight;
    pid_t loader_pid;
    struct timeval load_time;
};

struct conveyor {
    unsigned int K, M;
    unsigned int head, tail;
    unsigned int current_total_number_of_packages;
    unsigned int current_total_weight_of_packages;
    struct package packages[0]; // zero-length array
};

void parse_unsigned_int(char*, unsigned int*, const char*);

#endif // COMMON_H