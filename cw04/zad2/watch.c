#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>

#define error(...) { \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
    exit(EXIT_FAILURE); \
}

static volatile int active = 1; // don't optimize

char *buffer = NULL;
long size;
int copies = 0;
time_t last_mtime = 0;

int parse_int(char *str, int *int_ptr) {
    if (sscanf(str, "%d", int_ptr) != 1) {
        return -1;
    }
    return 0;
}

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

void handle_sigusr1() {
    active = (active == 0);
}

void handle_sigterm() {
    free(buffer);
    exit(copies); // return number of copies as status code
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        error("Usage: %s <file_path> <interval>", argv[0]);
    }

    char *rpath = realpath(argv[1], NULL);
    if (rpath == NULL) {
        error("File %s not found", argv[1]);
    }

    int interval;
    if (parse_int(argv[2], &interval) < 0) {
        error("Invalid interval for file %s", argv[1]);
    }
    if (interval <= 0) {
        error("Interval must be a positive integer");
    }
    
    active = 1;
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGINT, handle_sigterm);

    /* Proces kopiuje do pamięci zawartość monitorowanego pliku oraz datę jego ostatniej modyfikacji,
    po czym jeśli data modyfikacji pliku (a tym samym zawartość pliku) zostanie zmieniona,
    to proces zapisuje skopiowaną do pamięci starą wersję pliku we wspólnym dla wszystkich
    podkatalogu archiwum a do pamięci zapisuje aktualną wersję pliku. */
   
    while (1) {
        while (active) {
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

            sleep(interval);
        }    

        pause(); // wait for signal
    }
}
