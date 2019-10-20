#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef ZAD3
    #include <sys/time.h>
    #include <sys/resource.h>
#endif

static inline int min(int a, int b) {
    return a > b ? b : a;
}

#ifdef ZAD3
    static inline long timeval_to_microseconds(struct timeval tv) {
        return tv.tv_sec * 1000000L + tv.tv_usec;
    }
#endif

char* make_copy_path(char *rpath, time_t *mtime) {
    char *base = strdup(basename(rpath));
    size_t len = strlen(base);
    char *copypath = malloc(len+30); // strlen("archiwum/")+strlen("_RRRR-MM-DD_GG-MM-SS")+1 =30
    strcpy(copypath, "archiwum/");
    strcat(copypath, base);
    free(base);
    strftime(copypath+9+len, 21, "_%Y-%m-%d_%H-%M-%S", localtime(mtime));
    return copypath;
}

int get_file_mtime(char* rpath, time_t *mtime) {
    struct stat st;        
    if (stat(rpath, &st) == -1) {
        return -1;
    }
    *mtime = st.st_mtime;
    return 0;
}

int mode_memory(char *rpath, int duration, int interval) {
    /* Proces kopiuje do pamięci zawartość monitorowanego pliku oraz datę jego ostatniej modyfikacji,
    po czym jeśli data modyfikacji pliku (a tym samym zawartość pliku) zostanie zmieniona,
    to proces zapisuje skopiowaną do pamięci starą wersję pliku we wspólnym dla wszystkich
    podkatalogu archiwum a do pamięci zapisuje aktualną wersję pliku. */
   
    char *buffer = NULL;
    long size;

    int copies = 0;
    time_t last_mtime = 0;
    while (duration >= 0) {
        time_t mtime;
        if (get_file_mtime(rpath, &mtime) == -1) {
            return EXIT_FAILURE;
        }
        
        if (mtime != last_mtime) {
            // save the copy
            if (last_mtime != 0) {
                char* copypath = make_copy_path(rpath, &mtime);
                FILE *copy = fopen(copypath, "w");
                free(copypath);
                if (copy == NULL) {
                    fprintf(stderr, "Cannot create copy\n");
                    exit(EXIT_FAILURE);
                }            
                fwrite(buffer, 1, size, copy);
                fclose(copy);   
                copies++;
            }            
            last_mtime = mtime;  

            // read new version
            FILE* fp = fopen(rpath, "r");
            if (fp == NULL) {
                fprintf(stderr, "Cannot open file %s\n", rpath);
                exit(EXIT_FAILURE);
            }
            
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fseek(fp, 0, SEEK_SET); // rewind(fp);

            buffer = realloc(buffer, size); // realloc(NULL, size) <=> malloc(size)
            fread(buffer, 1, size, fp);
            fclose(fp);
        }

        sleep(min(interval, duration));
        duration -= interval;
    }
    
    free(buffer);
    return copies;
}

int mode_exec_cp(char *rpath, int duration, int interval) {    
    /* Plik nie jest zapisywany w pamięci, natomiast na samym początku oraz po każdej zanotowanej modyfikacji pliku,
    proces utworzy nowy proces, który wywoła jedną z funkcji z rodziny exec, aby wykonać cp. */

    int copies = 0;
    time_t last_mtime = 0;
    while (duration >= 0) {
        time_t mtime;
        if (get_file_mtime(rpath, &mtime) == -1) {
            return EXIT_FAILURE;
        }
        
        if (mtime != last_mtime) {
            last_mtime = mtime;

            switch (vfork()) {
                case -1: // fork failed
                    fprintf(stderr, "Fork failed\n");
                    return EXIT_FAILURE;

                case 0: // child process
                    execlp("cp", "cp", rpath, make_copy_path(rpath, &mtime), NULL);
                    // ...
                    fprintf(stderr, "Exec failed\n");
                    exit(EXIT_FAILURE);
            }

            // parent process
            copies++;
        }
        sleep(min(interval, duration));
        duration -= interval;
    }
    return copies;
}

int main(int argc, char *argv[]) {
    #ifdef ZAD3
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <file_path> <duration> <interval> <mode> <cpu_limit> <as_limit>\n", argv[0]);
    #else
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <file_path> <duration> <interval> <mode>\n", argv[0]);
    #endif
        exit(EXIT_FAILURE);
    }

    char *rpath = realpath(argv[1], NULL);
    if (rpath == NULL) {
        fprintf(stderr, "File %s not found\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    const int duration = atoi(argv[2]);
    if (duration <= 0) {
        fprintf(stderr, "Duration should be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    const int interval = atoi(argv[3]);
    if (interval <= 0) {
        fprintf(stderr, "Interval must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    #ifdef ZAD3
        const long cpu_limit = atoi(argv[5]);
        if (cpu_limit <= 0) {
            fprintf(stderr, "CPU time must be a positive integer\n");
            exit(EXIT_FAILURE);
        }

        const long as_limit = atoi(argv[6]);
        if (as_limit <= 0) {
            fprintf(stderr, "Virtual memory size must be a positive integer\n");
            exit(EXIT_FAILURE);
        }

        struct rlimit cpu_rlimit = {cpu_limit, cpu_limit}; // in seconds
        setrlimit(RLIMIT_CPU, &cpu_rlimit);

        struct rlimit as_rlimit = {as_limit * 1024 * 1024, as_limit * 1024 * 1024}; // in bytes, 1 MB = 2^10 kB = 2^20 B
        setrlimit(RLIMIT_AS, &as_rlimit);
    #endif

    int copies;
    const int mode = atoi(argv[4]);
    switch (mode) {
        case 1:
            copies = mode_memory(rpath, duration, interval); 
            break;

        case 2:
            copies = mode_exec_cp(rpath, duration, interval);
            break;

        default:     
            fprintf(stderr, "Invalid mode %d\n", mode);
            exit(EXIT_FAILURE);
    }

    #ifdef ZAD3
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        printf("pid %d, utime %ld μs, stime %ld μs, maxrss %ld, ru_minflt %ld, ru_majflt %ld, inblock %ld, oublock %ld, nvcsw %ld, nivcsw %ld \n",
            getpid(),
            timeval_to_microseconds(ru.ru_utime),
            timeval_to_microseconds(ru.ru_stime),
            ru.ru_maxrss, ru.ru_minflt, ru.ru_majflt, ru.ru_inblock, ru.ru_oublock, ru.ru_nvcsw, ru.ru_nivcsw
        );
    #endif

    exit(copies); // return number of copies as status code
}
