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

static volatile int enabled = 1;
// volatile => don't optimize

void print_datetime(void) {
    time_t now = time(NULL);

    #if CTIME // print datetime using ctime()
        printf("%s", ctime(&now));
    #elif STRFTIME // print datetime using strftime()
        static char buffer[20]; // strlen("YYYY-MM-DD HH:MM:SS")+1=20
        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
        printf("%s\n", buffer);
    #else
        #error "You must choose either CTIME or STRFTIME"
    #endif

    #ifdef DEBUG
        usleep(1e5);
    #endif
}

void handle_sigint() {
    printf("Odebrano sygnał SIGINT\n");
    exit(EXIT_SUCCESS);
}

void handle_sigtstp() {
    switch (enabled) {
        case 1:
            enabled = 0;
            printf("Oczekuję na Ctrl+Z - kontynuacja albo Ctrl+C - zakończenie programu\n");
            break;
        case 0:
            enabled = 1;
            putchar('\n');
    }
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

    #ifdef SIGSUSPEND
        sigset_t set;
        sigfillset(&set);
        sigdelset(&set, SIGINT);
        sigdelset(&set, SIGTSTP);
    #endif

    while (1) {
        while (enabled) {
            print_datetime();
        }

        #ifdef SIGSUSPEND
            sigsuspend(&set);
        #elif PAUSE
            pause();
        #else
            #error "You must choose either SIGSUSPEND or PAUSE"
        #endif
    }
}
