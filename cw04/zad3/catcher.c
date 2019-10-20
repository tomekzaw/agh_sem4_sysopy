#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        error("Usage: %s <mode>", argv[0]);
    }

    enum Mode mode = MODE_KILL;
    if (parse_mode(argv[1], &mode) < 0) {
        error("Invalid mode");
    }

    #ifndef CONFIRM
        // version with confirmation
        
        printf("Hello, I'm catcher (PID %d)\nwaiting for signals from sender...\n", getpid());

        pid_t sender_pid;
        int n;
        receive_sequence(&sender_pid, &n, NULL, mode);    
        printf("received %d signals from sender (PID %d)\n", n, sender_pid);

        printf("sending back %d signals to sender\n", n);
        send_sequence(sender_pid, n, mode);

    #else
        // version without confirmation

        printf("Hello, I'm catcher (PID %d)\nwaiting for signals from sender...\n", getpid());

        pid_t sender_pid;
        int n;
        receive_sequence_with_confirmation(&sender_pid, &n, NULL, mode);        
        printf("received and confirmed %d signals from sender (PID %d)\n", n, sender_pid);
        
    #endif
   
    return EXIT_SUCCESS;
}
