#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#ifdef ZAD3
    #include <sys/time.h>
    #include <sys/resource.h>

    static inline long timeval_to_microseconds(struct timeval tv) {
        return tv.tv_sec * 1000000L + tv.tv_usec;
    }
#endif

int main(int argc, char *argv[]) {
    #ifdef ZAD3
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <list_path> <duration> <mode> <cpu_limit> <as_limit>\n", argv[0]);
    #else
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <list_path> <duration> <mode>\n", argv[0]);
    #endif
        exit(EXIT_FAILURE);
    }

    const char *list_path = realpath(argv[1], NULL);
    if (list_path == NULL) {
        fprintf(stderr, "File %s not found\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    const char *duration = argv[2];
    const char *mode = argv[3];
    #ifdef ZAD3
        const char *cpu_limit = argv[4];
        const char *as_limit = argv[5];
    #endif

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open file %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    
    mkdir("archiwum", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // or: system("mkdir -p archiwum");

    char fpath[1024], interval[32];
    int nitems;
    while ((nitems = fscanf(fp, "%s %s\n", fpath, interval)) > 0) {
        if (nitems != 2) {
            continue;
        }

        pid_t child_pid = vfork();
        switch (child_pid) {
            case -1: // fork failed
                fprintf(stderr, "Fork failed\n");
                exit(EXIT_FAILURE);

            case 0: // child process
                #ifdef ZAD3
                    execlp("./watch", "watch", fpath, duration, interval, mode, cpu_limit, as_limit, NULL);
                #else
                    execlp("./watch", "watch", fpath, duration, interval, mode, NULL);
                #endif
                // ...
                fprintf(stderr, "Exec failed\n");
                exit(EXIT_FAILURE);

            // default: // parent process
        }

        #ifdef DEBUG
            printf("fpath=%s, duration=%s, interval=%s, mode=%s, PID=%d\n", fpath, duration, interval, mode, child_pid);
        #endif
    }

    fclose(fp);

    #ifdef ZAD3
        struct rusage old_usage;
        getrusage(RUSAGE_CHILDREN, &old_usage);
    #endif

    pid_t pid;
    int wstatus;
    while ((pid = wait(&wstatus)) != -1) {
        #ifdef ZAD3
            struct rusage usage;
            getrusage(RUSAGE_CHILDREN, &usage);
        #endif
        
        if (WIFEXITED(wstatus)) {
            int copies = WEXITSTATUS(wstatus);
            #ifdef ZAD3
                struct timeval utime;
                struct timeval stime;
                timersub(&usage.ru_utime, &old_usage.ru_utime, &utime);
                timersub(&usage.ru_stime, &old_usage.ru_stime, &stime);
                printf("Proces %d utworzył %d kopii pliku: utime %ld μs, stime %ld μs\n", \
                    pid, copies, timeval_to_microseconds(utime), timeval_to_microseconds(stime));
            #else
                printf("Proces %d utworzył %d kopii pliku\n", pid, copies);
            #endif
        }

        #ifdef ZAD3
            old_usage = usage;
        #endif
    }

    exit(EXIT_SUCCESS);
}
