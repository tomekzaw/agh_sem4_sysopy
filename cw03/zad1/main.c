#define _XOPEN_SOURCE 500

#ifdef _WIN32
    #define PATH_SEPARATOR "\\" 
#else 
    #define PATH_SEPARATOR "/" 
#endif

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

int is_dir(const char *path) {
    struct stat st;        
    if (stat(path, &st) == -1) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

char *make_path(const char *dirpath, const char *filename) {
    char *filepath = malloc(strlen(dirpath) + strlen(filename) + strlen(PATH_SEPARATOR) + 1);
    if (!filepath) {
        return NULL;
    }
    strcpy(filepath, dirpath);
    strcat(filepath, PATH_SEPARATOR);    
    return strcat(filepath, filename);
}

static int baselen;

#ifdef NFTW // nftw() version
    static int display_info(const char *filepath, __attribute__((unused)) const struct stat *st, int tflag, struct FTW *ftwbuf) {
        if (tflag != FTW_D) {
            return 0; // only directories
        }

        if (ftwbuf->level == 0) {
            return 0; // skip base directory
        }

        switch (vfork()) { // instead of fork()
            case -1: // fork failed
                fprintf(stderr, "Fork failed\n");
                return -1;
            
            case 0: // child process
                printf("\n\033[34m%s\033[0m\n\033[36m%d\033[0m\n", filepath+baselen, getpid());
                chdir(filepath);
                execlp("ls", "ls", "-l", NULL);
                // or: execlp("ls", "ls", "-l", filepath, NULL);
        }

        #ifdef WAIT
            wait(NULL); // wait for child process
        #endif

        return 0;
    }

    int list(const char *dirpath) {
        if (nftw(dirpath, display_info, 20, FTW_PHYS) == -1) {
            fprintf(stderr, "Cannot walk directory tree for %s\n", dirpath);
            return -1;
        }
        
        #ifndef WAIT
            while (wait(NULL) != -1); // wait for all child processes
        #endif

        return 0;
    }
#else
    // opendir(), closedir(), stat() version
    static int _list(const char *dirpath) {
        DIR *dirp = opendir(dirpath);
        if (!dirp) {
            fprintf(stderr, "Cannot open directory %s\n", dirpath);
            return -1;
        }

        struct dirent *dirent;
        while ((dirent = readdir(dirp))) {
            const char *filename = dirent->d_name;
            if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
                continue;
            }

            char *filepath = make_path(dirpath, filename);

            struct stat st;
            if (lstat(filepath, &st)) { // instead of stat()
                fprintf(stderr, "Cannot get status for %s\n", filepath);
                return -1;
            }

            if (S_ISDIR(st.st_mode)) {
                switch (vfork()) {
                    case -1: // fork failed
                        fprintf(stderr, "Fork failed\n");
                        return -1;
                    
                    case 0: // child process
                        chdir(filepath);
                        printf("\n\033[34m%s\033[0m\n\033[36m%d\033[0m\n", filepath, getpid());
                        execlp("ls", "ls", "-l", NULL); // or: execlp("ls", "ls", "-l", filepath, NULL);
                }
                #ifdef WAIT
                    wait(NULL); // wait for child process
                #endif
                _list_recursive(filepath); // recursive call
            }
            free(filepath);
        }
        if (closedir(dirp)) {
            fprintf(stderr, "Cannot close directory\n");
            return -1;
        }
        return 0;
    }

    int list(const char* dirpath) {
        int result = _list_recursive(dirpath);
        #ifndef WAIT
            while (wait(NULL) != -1); // wait for all child processes
        #endif
        return result;
    }
#endif

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Directory not specified\n");
        return -1;
    }

    char *dirpath = realpath(argv[1], NULL);
    if (!dirpath) {
        fprintf(stderr, "Directory %s not found\n", argv[1]);
        return -1;
    }
    if (!is_dir(dirpath)) {
        fprintf(stderr, "%s is not a directory\n", dirpath);
        return -1;
    }
    
    baselen = strlen(dirpath)+1;
    list(dirpath);
    free(dirpath);

    printf("\n");
    return 0;
}
