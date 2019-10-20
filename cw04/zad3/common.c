#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "common.h"

static volatile int counter, listen; // don't optimize
static pid_t sender;
static int value;

int parse_int(char *str, int *int_ptr) {
    if (sscanf(str, "%d", int_ptr) != 1) {
        return -1;
    }
    return 0;
}

int parse_pid(char *str, pid_t *pid_ptr) {
    if (sscanf(str, "%d", pid_ptr) != 1 || *pid_ptr < 0) {
        return -1;
    }
    return 0;
}

int parse_mode(char *str, enum Mode *mode_ptr) {    
    if (strcmp(str, "KILL") == 0) {
        *mode_ptr = MODE_KILL;
        return 0;
    }
    if (strcmp(str, "SIGQUEUE") == 0) {
        *mode_ptr = MODE_SIGQUEUE;
        return 0;
    }
    if (strcmp(str, "SIGRT") == 0) {
        *mode_ptr = MODE_SIGRT;
        return 0;
    }
    return -1;
}

void send_sequence(pid_t pid_to, int n, enum Mode mode) {
    int i;
    for (i = 0; i < n; i++) {
        switch (mode) {
            case MODE_KILL:
                kill(pid_to, SIGUSR1);
                break;
            case MODE_SIGQUEUE:
                sigqueue(pid_to, SIGUSR1, (union sigval) {i});
                break;
            case MODE_SIGRT:
                kill(pid_to, SIGRT1);
                break;
        }
    }

    switch (mode) {
        case MODE_KILL:
            kill(pid_to, SIGUSR2);
            break;
        case MODE_SIGQUEUE:
            sigqueue(pid_to, SIGUSR2, (union sigval) {n});
            break;
        case MODE_SIGRT:
            kill(pid_to, SIGRT2);
            break;
    }
}

void receive_handle_signal(int signo, siginfo_t* info, __attribute__((unused)) void* context) {
    if (signo == SIGUSR1 || signo == SIGRT1) {
        counter++;
    } else if (signo == SIGUSR2 || signo == SIGRT2) {
        listen = 0;
    }
    sender = info->si_pid;
    value = info->si_value.sival_int;
}

void receive_sequence(pid_t *pid_from_ptr, int *n_ptr, int *value_ptr, enum Mode mode) {
    sigset_t newmask, oldmask;
    sigfillset(&newmask);
    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            sigdelset(&newmask, SIGUSR1);
            sigdelset(&newmask, SIGUSR2);
            break;
        case MODE_SIGRT:
            sigdelset(&newmask, SIGRT1);
            sigdelset(&newmask, SIGRT2);
    }
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        error("Cannot set signal mask");
    }

    struct sigaction act;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = receive_handle_signal;
    sigemptyset(&act.sa_mask);
    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            sigaction(SIGUSR1, &act, NULL);
            sigaction(SIGUSR2, &act, NULL);
            break;
        case MODE_SIGRT:
            sigaction(SIGRT1, &act, NULL);
            sigaction(SIGRT2, &act, NULL);
    }

    counter = 0;
    listen = 1;
    while (listen) {
        pause(); // wait for signals
    }

    if (sigprocmask(SIG_SETMASK, &newmask, NULL) < 0) {
        error("Cannot reset signal mask");
    }

    if (pid_from_ptr != NULL) {
        *pid_from_ptr = sender;
    }
    if (n_ptr != NULL) {
        *n_ptr = counter;
    }
    if (value_ptr != NULL) {
        *value_ptr = value;
    }
}

void do_nothing() {}

void send_sequence_with_confirmation(pid_t pid_to, int n, enum Mode mode) {
    sigset_t newmask, oldmask;
    sigfillset(&newmask);
    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            sigdelset(&newmask, SIGUSR1);
            sigdelset(&newmask, SIGUSR2);
            break;
        case MODE_SIGRT:
            sigdelset(&newmask, SIGRT1);
            sigdelset(&newmask, SIGRT2);
    }
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        error("Cannot set signal mask");
    }

    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            signal(SIGUSR1, do_nothing);
            signal(SIGUSR2, do_nothing);
            break;
        case MODE_SIGRT:
            signal(SIGRT1, do_nothing);
            signal(SIGRT2, do_nothing);
            break;
    }

    int i;
    for (i = 0; i < n; i++) {
        // send signal
        switch (mode) {
            case MODE_KILL:
                kill(pid_to, SIGUSR1);
                break;
            case MODE_SIGQUEUE:
                sigqueue(pid_to, SIGUSR1, (union sigval) {i});
                break;
            case MODE_SIGRT:
                kill(pid_to, SIGRT1);
                break;
        }

        // wait for confirmation
        pause();
    }

    // send signal
    switch (mode) {
        case MODE_KILL:
            kill(pid_to, SIGUSR2);
            break;
        case MODE_SIGQUEUE:
            sigqueue(pid_to, SIGUSR2, (union sigval) {n});
            break;
        case MODE_SIGRT:
            kill(pid_to, SIGRT2);
            break;
    }

    // wait for confirmation
    pause();

    if (sigprocmask(SIG_SETMASK, &newmask, NULL) < 0) {
        error("Cannot reset signal mask");
    }
}

void receive_sequence_with_confirmation(pid_t *pid_from_ptr, int *n_ptr, int *value_ptr, enum Mode mode) {
    sigset_t newmask, oldmask;
    sigfillset(&newmask);
    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            sigdelset(&newmask, SIGUSR1);
            sigdelset(&newmask, SIGUSR2);
            break;
        case MODE_SIGRT:
            sigdelset(&newmask, SIGRT1);
            sigdelset(&newmask, SIGRT2);
    }
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        error("Cannot set signal mask");
    }

    struct sigaction act;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = receive_handle_signal;
    sigemptyset(&act.sa_mask);
    switch (mode) {
        case MODE_KILL:
        case MODE_SIGQUEUE:
            sigaction(SIGUSR1, &act, NULL);
            sigaction(SIGUSR2, &act, NULL);
            break;
        case MODE_SIGRT:
            sigaction(SIGRT1, &act, NULL);
            sigaction(SIGRT2, &act, NULL);
    }

    counter = 0;
    listen = 1;
    int i = 0;
    while (listen) {
        // wait for signal
        pause();

        // send confirmation
        if (listen) {
            switch (mode) {
                case MODE_KILL:
                    kill(sender, SIGUSR1);
                    break;
                case MODE_SIGQUEUE:
                    sigqueue(sender, SIGUSR1, (union sigval) {i++});
                    break;
                case MODE_SIGRT:
                    kill(sender, SIGRT1);
                    break;
            }
        } else {
            switch (mode) {
                case MODE_KILL:
                    kill(sender, SIGUSR2);
                    break;
                case MODE_SIGQUEUE:
                    sigqueue(sender, SIGUSR2, (union sigval) {i});
                    break;
                case MODE_SIGRT:
                    kill(sender, SIGRT2);
                    break;
            }
        }
    }

    if (sigprocmask(SIG_SETMASK, &newmask, NULL) < 0) {
        error("Cannot reset signal mask");
    }

    if (pid_from_ptr != NULL) {
        *pid_from_ptr = sender;
    }
    if (n_ptr != NULL) {
        *n_ptr = counter;
    }
    if (value_ptr != NULL) {
        *value_ptr = value;
    }
}