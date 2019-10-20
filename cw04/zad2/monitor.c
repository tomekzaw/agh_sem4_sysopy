#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

#define error(...) { \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
    exit(EXIT_FAILURE); \
}

struct proc {
    char *fpath;
    int interval;
    pid_t pid;
    int active;
} *proclist[1024];

int nproc;

void strtoupper(char *str) {
    while (*str) {
        *str = toupper(*str);
        str++;
    }
}

void list() {
    printf("\x1b[1;37m%-16s%-12s%-8s%-8s\x1b[0m\n", "path", "interval", "pid", "active");
    int i;
    for (i = 0; i < nproc; i++) {
        struct proc *proc = proclist[i];
        printf("%-16.16s%7ds    %-8d%-8s\n", proc->fpath, proc->interval, proc->pid, proc->active ? "\x1b[1;32mYes\x1b[0m" : "\x1b[1;31mNo\x1b[0m");
    }
}

void set(pid_t pid, int active) {
    int i;
    for (i = 0; i < nproc; i++) {
        struct proc *proc = proclist[i];
        if (proc->pid == pid) {
            active = (active == 1);
            if (proc->active == active) {
                printf("Process PID %d has already been %s\n", pid, active ? "started" : "stopped");
            } else {
                proc->active = (active == 1);
                printf("Process PID %d %s\n", pid, active ? "started" : "stopped");
                kill(proc->pid, SIGUSR1);
            }
            return;
        }
    }
    fprintf(stderr, "PID %d not found\n", pid);
}

void start(pid_t pid) {
    set(pid, 1);
}

void stop(pid_t pid) {
    set(pid, 0);
}

void set_all(int active) {
    active = (active == 1);
    int i;
    for (i = 0; i < nproc; i++) {
        struct proc *proc = proclist[i];
        if (proc->active == active) {
            printf("Process PID %d has already been %s\n", proc->pid, active ? "started" : "stopped");
        } else {
            proc->active = (active == 1);
            printf("Process PID %d %s\n", proc->pid, active ? "started" : "stopped");
            kill(proc->pid, SIGUSR1);
        }
    }
}

void start_all() {
    set_all(1);
}

void stop_all() {
    set_all(0);
}

void end() {
    putchar('\n');
    int i;
    for (i = 0; i < nproc; i++) {
        struct proc *proc = proclist[i];
        kill(proc->pid, SIGINT);

        int status;
        waitpid(proc->pid, &status, 0);
        if (WIFEXITED(status)) {
            int copies = WEXITSTATUS(status);
            printf("Proces %d utworzył %d kopii pliku %s\n", proc->pid, copies, proc->fpath);
        }

        free(proc);
    }
    putchar('\n');
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        error("Usage: %s <list_path>", argv[0]);
    }

    const char *list_path = realpath(argv[1], NULL);
    if (list_path == NULL) {
        error("File %s not found", argv[1]);
    }

    FILE *fp = fopen(list_path, "r");
    if (fp == NULL) {
        error("Cannot open file %s", argv[1]);
    }
    
    mkdir("archiwum", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // or: system("mkdir -p archiwum");

    {
        char fpath[1024], interval[32];
        int nitems, i = 0;
        while ((nitems = fscanf(fp, "%s %s\n", fpath, interval)) > 0) {
            if (nitems != 2) {
                continue;
            }

            pid_t child_pid = vfork();
            switch (child_pid) {
                case -1: // fork failed
                    error("Fork failed");

                case 0: // child process
                    execlp("./watch", "watch", fpath, interval, NULL);
                    error("Exec failed");
            }

            struct proc *proc = proclist[i++] = malloc(sizeof(struct proc));
            proc->fpath = strdup(fpath);
            proc->interval = atoi(interval);
            proc->pid = child_pid;
            proc->active = 1;
        }
        nproc = i;
    }

    fclose(fp);

    signal(SIGINT, end);

    list();

    while (1) {
        char line[1024];
        printf("\n> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            error("Cannot read command");
        }
        line[strlen(line)-1] = '\0';
        strtoupper(line);
        
        if (strcmp(line, "LIST") == 0) {
            list();
            continue;
        }
        if (strcmp(line, "STOP ALL") == 0) {
            stop_all();
            continue;
        }
        if (strcmp(line, "START ALL") == 0) {
            start_all();
            continue;
        }
        if (strcmp(line, "END") == 0) {
            end();
        }
        char cmd[1024];
        int arg;
        if (sscanf(line, "%s %d", cmd, &arg) == 2) {
            if (strcmp(cmd, "STOP") == 0) {
                stop((pid_t) arg);
                continue;
            }
            if (strcmp(cmd, "START") == 0) {
                start((pid_t) arg);
                continue;
            }
        }
        fprintf(stderr, "Invalid command\n");
    }
}
