#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define error(...) { \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
    exit(EXIT_FAILURE); \
}

static volatile pid_t child_pid = 0;
// volatile => don't optimize
// PID 0 = swapper => safe value

void spawn_child(void) {
    switch (child_pid = vfork()) {
        case -1: // fork failed
            error("Fork failed");

        case 0: // child process
            execl("./date.sh", "date.sh", NULL);
            fprintf(stderr, "Exec failed\n\nBash script cannot be executed.\nProbably it does not have rights to run.\nPlease type the following command:\n$ \x1b[1;37mchmod u+x date.sh\x1b[0m\nand try again.\n\n");
            kill(getppid(), SIGTERM);
            exit(EXIT_FAILURE);
    }
}

void kill_child(void) {
    kill(child_pid, SIGKILL);
    child_pid = 0; // PID 0 = swapper -> safe value
}

void handle_sigtstp() {
    switch (child_pid) {
        case 0: // no child
            spawn_child();
            break;
        default: // has child
            kill_child();
            printf("Oczekuję na Ctrl+Z - kontynuacja albo Ctrl+C - zakończenie programu\n");
            // note: can't use pause() in signal handler!
    }
}

void handle_sigint() {
    printf("Odebrano sygnał SIGINT\n");
    if (child_pid != 0) {
        kill_child();
    }
    exit(EXIT_SUCCESS);
}

void handle_sigterm() {
    exit(EXIT_FAILURE); // for exec error handling
}

int main(void) {
    // handle SIGINT using signal(2)
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        error("Cannot register signal handler for SIGINT");
    }

    // handle SIGTSTP using sigaction(2)
    struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = handle_sigtstp;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGTSTP, &act, NULL) < 0) {
        error("Cannot register signal handler for SIGTSTP");
    }

    signal(SIGTERM, handle_sigterm);
    #ifdef SIGSUSPEND
        sigset_t set;
        sigfillset(&set);
        sigdelset(&set, SIGINT);
        sigdelset(&set, SIGTSTP);
    #endif

    spawn_child();
    while (1) {
        #ifdef SIGSUSPEND
            sigsuspend(&set);
        #elif PAUSE
            pause();
        #else
            #error "You must choose either SIGSUSPEND or PAUSE"
        #endif
    }
}
