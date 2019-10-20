#define _XOPEN_SOURCE 500

#ifdef _WIN32
    #define PATH_SEPARATOR "\\" 
#else 
    #define PATH_SEPARATOR "/" 
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>
#include <string.h>
#include <time.h>

enum Operator {BEFORE = -1, THEN = 0, AFTER = 1};

static enum Operator op;
static time_t mtime;

int parse_operator(const char *str, enum Operator *op) {
    if (strcmp(str, "<") == 0) {
        *op = BEFORE;
        return 0;
    }
    if (strcmp(str, "=") == 0) {
        *op = THEN;
        return 0;
    }
    if (strcmp(str, ">") == 0) {
        *op = AFTER;
        return 0;
    }
    return -1;
}

int compare_time(time_t t1, enum Operator op, time_t t2) {
    double seconds = difftime(t1, t2); // t1 - t2
    switch (op) {
        case BEFORE:
            return (seconds < 0);
        case THEN:
            return (seconds == 0);
        case AFTER:
            return (seconds > 0);
    }
    return -1;
}

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

const char *format_mode(mode_t st_mode) {
    // http://www.gnu.org/software/libc/manual/html_node/Testing-File-Type.html
    if (S_ISREG(st_mode)) return "file";
    if (S_ISDIR(st_mode)) return "dir";
    if (S_ISCHR(st_mode)) return "char dev";
    if (S_ISBLK(st_mode)) return "block dev";
    if (S_ISFIFO(st_mode)) return "fifo";
    if (S_ISLNK(st_mode)) return "slink";
    if (S_ISSOCK(st_mode)) return "sock";
    return "?";
}

char *format_time(time_t datetime) {
    char *str = malloc(20); // strlen("YYYY-MM-DD HH:MM:SS") + 1 = 20, sizeof(char) = 1
    strftime(str, 20, "%Y-%m-%d %H:%M:%S", localtime(&datetime));
    return str;
}

static int display_info(const char *filepath, const struct stat *st, __attribute__((unused)) int tflag, struct FTW *ftwbuf) {
    if (ftwbuf && ftwbuf->level == 0) {
        return 0; // skip main directory
    }

    if (compare_time(st->st_mtime, op, mtime) == 1) {
        char *st_atime_str = format_time(st->st_atime);
        char *st_mtime_str = format_time(st->st_mtime);
        printf("%-64.60s%-10s%-10zu%-22s%-22s\n",
            filepath,
            format_mode(st->st_mode),
            st->st_size,
            st_atime_str,
            st_mtime_str
        );
        free(st_atime_str);
        free(st_mtime_str);
    }
    return 0;
}

int list_dir_nftw(const char *dirpath) {
    if (nftw(dirpath, display_info, 20, FTW_PHYS) == -1) {
        fprintf(stderr, "Cannot walk directory tree\n");
        return -1;
    }
    return 0;
}

int list_dir_stat(const char *dirpath) {
    DIR *dirp = opendir(dirpath);
    if (!dirp) {
        fprintf(stderr, "Cannot open directory\n");
        return -1;
    }

    // rewinddir(dirp);
    struct dirent *dirent;
    while ((dirent = readdir(dirp))) {
        const char *filename = dirent->d_name;
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            continue;
        }

        char *filepath = make_path(dirpath, filename);

        struct stat st;
        if (lstat(filepath, &st)) { // instead of stat()
            fprintf(stderr, "Cannot get file status\n");
            return -1;
        }

        display_info(filepath, &st, 1, NULL);

        if (S_ISDIR(st.st_mode)) {
            list_dir_stat(filepath); // recursive call
        }

        free(filepath);
    }

    if (closedir(dirp)) {
        fprintf(stderr, "Cannot close directory\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Invalid arguments\n\
Syntax: ./main <dirpath> <operator> <date>\n\
    <dirpath>    path to directory\n\
    <operator>   \"<\", \"=\", or \">\"\n\
    <date>       date in format YYYY-MM-DD HH:MM:SS\n\n");
        return -1;
    }

    char *dirpath = realpath(argv[1], NULL);
    if (!dirpath) {
        fprintf(stderr, "Directory %s not found\n", dirpath);
        return -1;
    }
    if (!is_dir(dirpath)) {
        fprintf(stderr, "%s is not a directory\n", dirpath);
        return -1;
    }

    if (parse_operator(argv[2], &op) == -1) {
        fprintf(stderr, "Invalid operator\n");
        return -1;
    }

    struct tm *mtime_tm = calloc(1, sizeof(struct tm)); // properly initializes struct tm
    if (strptime(argv[3], "%Y-%m-%d %H:%M:%S", mtime_tm) == NULL) {
        fprintf(stderr, "Invalid date\n");
        return -1;
    }
    mtime = mktime(mtime_tm);
    
    #ifdef NFTW
        list_dir_nftw(dirpath);
    #else
        list_dir_stat(dirpath);
    #endif

    free(dirpath);
    return 0;
}
