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
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SEMAPHORE_NAME "/tzawadzk_sysopy_cw07_zad2"
#define SHARED_MEMORY_NAME "/tzawadzk_sysopy_cw07_zad2"

struct package {
    unsigned int weight;
    pid_t loader_pid;
    struct timeval load_time;
};

struct conveyor {
    off_t shm_size;
    unsigned int K, M;
    unsigned int head, tail;
    unsigned int current_total_number_of_packages;
    unsigned int current_total_weight_of_packages;
    struct package packages[0]; // zero-length array
};

void parse_unsigned_int(char*, unsigned int*, const char*);

#endif // COMMON_H