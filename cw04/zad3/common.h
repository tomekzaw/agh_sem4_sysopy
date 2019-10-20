#ifndef _COMMON_H
#define _COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define error(...) { \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
    exit(EXIT_FAILURE); \
}

#define SIGRT1 SIGRTMIN+1
#define SIGRT2 SIGRTMIN+2

enum Mode {MODE_KILL, MODE_SIGQUEUE, MODE_SIGRT};

int parse_int(char*, int*);
int parse_pid(char*, pid_t*);
int parse_mode(char*, enum Mode*);

void send_sequence(pid_t, int, enum Mode);
void receive_sequence(pid_t*, int*, int*, enum Mode);

void send_sequence_with_confirmation(pid_t, int, enum Mode);
void receive_sequence_with_confirmation(pid_t*, int*, int*, enum Mode);

#endif