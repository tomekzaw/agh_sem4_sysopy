#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        error("Usage: %s <catcher PID> <number of signals> <mode>", argv[0]);
    }

    pid_t catcher_pid;
    if (parse_pid(argv[1], &catcher_pid) < 0) {
        error("Invalid catcher PID");
    }
    
    int n_send;
    if (parse_int(argv[2], &n_send) < 0) {
        error("Invalid number of signals");
    }
    if (n_send < 0) {
        error("Number of signals must be a positive integer");
    }

    enum Mode mode = MODE_KILL;
    if (parse_mode(argv[3], &mode) < 0) {
        error("Invalid mode");
    }

    #ifndef CONFIRM
        // version with confirmation

        printf("Hello, I'm sender (PID %d)\nsending %d signals to catcher (PID %d)\n", getpid(), n_send, catcher_pid);
        send_sequence(catcher_pid, n_send, mode);

        int n_sender_received;
        int *n_catcher_received = NULL;
        if (mode == MODE_SIGQUEUE) {
            n_catcher_received = malloc(sizeof(int));
        }
        receive_sequence(NULL, &n_sender_received, n_catcher_received, mode);
        if (mode == MODE_SIGQUEUE) {
            printf("catcher has received %d signals (%.2f%%)\n", *n_catcher_received, (100.0 * *n_catcher_received / n_send));
            free(n_catcher_received);
        }
        printf("received back %d signals from catcher (%.2f%%)\n", n_sender_received, (100.0 * n_sender_received / n_send));

    #else
        // version without confirmation

        printf("Hello, I'm sender (PID %d)\nsending %d signals with confirmation to catcher (PID %d)\n", getpid(), n_send, catcher_pid);
        send_sequence_with_confirmation(catcher_pid, n_send, mode);
        printf("sent and received confirmation for %d signals\n", n_send);
        
    #endif
    
    return EXIT_SUCCESS;
}
