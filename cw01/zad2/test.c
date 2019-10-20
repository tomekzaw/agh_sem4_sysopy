#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>

#ifdef DYNAMIC
    #include <dlfcn.h>
#else
    #include "../zad1/blocks.h"
#endif


static clock_t clock_t_start, clock_t_end;
static struct timespec timespec_start, timespec_end;
static struct tms tms_start, tms_end;

double calc_timespec_time(struct timespec start, struct timespec end) {
    return ((double) end.tv_sec - (double) start.tv_sec) + 1e-9 * ((double) end.tv_nsec - (double) start.tv_nsec);
}

double calc_clock_t_time(clock_t start, clock_t end) {
    return (intmax_t) (end - start) / (double) sysconf(_SC_CLK_TCK);
}

void start_clock(void) {
    clock_t_start = times(&tms_start);
    clock_gettime(CLOCK_REALTIME, &timespec_start);
}

void end_clock(void) {
    clock_t_end = times(&tms_end);
    clock_gettime(CLOCK_REALTIME, &timespec_end);
}

void print_times(void) {           
    printf("real %10.7fs\treal %6.3fs\tuser %6.3fs\tsys %6.3fs",
        calc_timespec_time(timespec_start, timespec_end),
        calc_clock_t_time(clock_t_start, clock_t_end),
        calc_clock_t_time(tms_start.tms_cutime, tms_end.tms_cutime),
        calc_clock_t_time(tms_start.tms_cstime, tms_end.tms_cstime));
}

int main(int argc, char* argv[]) {
    #ifdef DYNAMIC
        void *handle = dlopen("../zad1/libblocks.so", RTLD_LAZY);
        if (!handle) {
            fprintf(stderr, "Cannot load dynamic library libblocks.so\n");
            exit(1);
        }
        
        void (*create_table)(int) = (void (*)(int)) dlsym(handle, "create_table");
        void (*set_current_directory)(char*) = (void (*)(char*)) dlsym(handle, "set_current_directory");
        void (*set_current_file)(char*) = (void (*)(char*)) dlsym(handle, "set_current_file");
        void (*set_current_temp_file)(char*) = (void (*)(char*)) dlsym(handle, "set_current_temp_file");
        void (*search_directory)(void) = (void (*)(void)) dlsym(handle, "search_directory");
        void (*create_block)(void) = (void (*)(void)) dlsym(handle, "create_block");
        void (*remove_block)(int) = (void (*)(int)) dlsym(handle, "remove_block");
        void (*free_table)(void) = (void (*)(void)) dlsym(handle, "free_table");

        if (dlerror()) {
            fprintf(stderr, "Cannot load functions from dynamic library libblocks.so\n");
            exit(1);
        }
    #endif

    if (argc == 1) {
        fprintf(stderr, "Table size not specified\n");
        exit(1);
    }

    int table_size = atoi(argv[1]);
    create_table(table_size);

    int i;
    for (i = 2; i < argc; i++) {
        char* cmd = argv[i];

        if (strcmp(cmd, "search_directory") == 0) {
           
            // search_directory dir file name_file_temp
            if (i+3 >= argc) {
                fprintf(stderr, "Too few arguments to search_directory\nSyntax: %s dir file file_temp\n", cmd);
                exit(3);
            }

            char* dir = argv[++i];
            char* file = argv[++i];
            char* temp_file = argv[++i];
            set_current_directory(dir);
            set_current_file(file);
            set_current_temp_file(temp_file);

            printf("> %s %s %s %s\n", cmd, dir, file, temp_file);

            start_clock();
            search_directory();
            end_clock();
            print_times();
            printf("\tsearch_directory\n");

            start_clock();
            create_block();
            end_clock();
            print_times();
            printf("\tcreate_block\n");

        } else if (strcmp(cmd, "remove_block") == 0) {
            
            // remove_block index
            if (i+1 >= argc) {
                fprintf(stderr, "Too few arguments to remove_block\nSyntax: %s index\n", cmd);
                exit(4);
            }

            int index = atoi(argv[++i]);

            printf("> %s %d\n", cmd, index);

            start_clock();
            remove_block(index);
            end_clock();
            print_times();
            printf("\tremove_block\n");

        } else {
            fprintf(stderr, "Unknown task \"%s\" in operation list\n", argv[i]);
            exit(2);
        }
        
        printf("\n");
    }
    
    free_table();

    #ifdef DYNAMIC
        dlclose(handle);
    #endif

    return 0;
}
