#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

unsigned char random_byte(void) {
    #ifdef DEBUG
        return rand() % ('9' - '0' + 1) + '0';
    #else
        return rand();
    #endif
}

int generate_sys(const char *path, const int nrecords, const int nbytes) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open file\n");
        return -1;
    }

    int r;
    for (r = 0; r < nrecords; r++) {
        int b;
        for (b = 0; b < nbytes; b++) {
            unsigned char c = random_byte();
            if (write(fd, &c, 1) != 1) {
                fprintf(stderr, "Cannot write to file\n");
                return -1;
            }        
        }
    }

    if (close(fd) != 0) {
        fprintf(stderr, "Cannot close file\n");
        return -1;
    }

    return 0;
}

int generate_lib(const char *path, const int nrecords, const int nbytes) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Cannot open file\n");
        return -1;
    }

    int r;
    for (r = 0; r < nrecords; r++) {
        int b;
        for (b = 0; b < nbytes; b++) {
            unsigned char c = random_byte();
            if (fwrite(&c, 1, 1, fp) != 1) {
                fprintf(stderr, "Cannot write to file\n");
                return -1;
            }        
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Cannot close file\n");
        return -1;
    }

    return 0;
}

/*
void selection_sort(int* tab, int n) {
    for (int i = 0; i < n-1; i++) {
        int min = i;
        for (int j = i; j < n; j++) {
            if (tab[j] < tab[min]) {
                min = j;
            }
        }
        int tmp = tab[i];
        tab[i] = tab[min];
        tab[min] = tmp;
    }
}
*/

int sort_sys(const char *path, const int nrecords, const int nbytes) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open file\n");
        return -1;
    }

    int i;
    for (i = 0; i < nrecords-1; i++) {
        int min = i;
        unsigned char min_c;
        lseek(fd, i * nbytes, SEEK_SET);
        read(fd, &min_c, 1);

        int j;
        for (j = i+1; j < nrecords; j++) {
            unsigned char c;
            lseek(fd, j * nbytes, SEEK_SET);
            read(fd, &c, 1);
            if (c < min_c) {
                min = j;
                min_c = c;
            }
        }

        if (i != min) {
            char *buf_i = malloc(nbytes);
            lseek(fd, i * nbytes, SEEK_SET);
            read(fd, buf_i, nbytes);

            char *buf_min = malloc(nbytes);
            lseek(fd, min * nbytes, SEEK_SET);
            read(fd, buf_min, nbytes);

            lseek(fd, i * nbytes, SEEK_SET);
            write(fd, buf_min, nbytes);
            free(buf_min);

            lseek(fd, min * nbytes, SEEK_SET);
            write(fd, buf_i, nbytes);
            free(buf_i);
        }
    }

    if (close(fd) != 0) {
        fprintf(stderr, "Cannot close file\n");
        return -1;
    }

    return 0;
}

int sort_lib(const char *path, const int nrecords, const int nbytes) {
    FILE *fp = fopen(path, "r+");
    if (!fp) {
        fprintf(stderr, "Cannot open file\n");
        return -1;
    }

    int i;
    for (i = 0; i < nrecords-1; i++) {
        int min = i;
        unsigned char min_c;
        fseek(fp, i * nbytes, SEEK_SET);
        fread(&min_c, 1, 1, fp);

        int j;
        for (j = i+1; j < nrecords; j++) {
            unsigned char c;
            fseek(fp, j * nbytes, SEEK_SET);
            fread(&c, 1, 1, fp);
            if (c < min_c) {
                min = j;
                min_c = c;
            }
        }

        if (i != min) {
            char *buf_i = malloc(nbytes);
            fseek(fp, i * nbytes, SEEK_SET);
            fread(buf_i, 1, nbytes, fp);

            char *buf_min = malloc(nbytes);
            fseek(fp, min * nbytes, SEEK_SET);
            fread(buf_min, 1, nbytes, fp);

            fseek(fp, i * nbytes, SEEK_SET);
            fwrite(buf_min, 1, nbytes, fp);
            free(buf_min);

            fseek(fp, min * nbytes, SEEK_SET);
            fwrite(buf_i, 1, nbytes, fp);
            free(buf_i);
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Cannot close file\n");
        return -1;
    }

    return 0;
}

int copy_sys(const char *path_src, const char *path_dest, const int nrecords, const int nbytes) {    
    int src = open(path_src, O_RDONLY);
    if (src < 0) {
        fprintf(stderr, "Cannot open source file\n");
        return -1;
    }    
    
    int dest = open(path_dest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (dest < 0) {
        fprintf(stderr, "Cannot open destination file\n");
        return -1;
    }

    char *buf = malloc(nbytes);
    int r;
    for (r = 0; r < nrecords; r++) {
        read(src, buf, nbytes);
        write(dest, buf, nbytes);
    }
    free(buf);

    if (close(src) != 0) {
        fprintf(stderr, "Cannot close source file\n");
        return -1;
    }
    if (close(dest) != 0) {
        fprintf(stderr, "Cannot close destination file\n");
        return -1;
    }

    return 0;
}

int copy_lib(const char *path_src, const char *path_dest, const int nrecords, const int nbytes) {
    FILE *src = fopen(path_src, "r");
    if (!src) {
        fprintf(stderr, "Cannot open source file\n");
        return -1;
    }

    FILE *dest = fopen(path_dest, "w");
    if (!dest) {
        fprintf(stderr, "Cannot open destination file\n");
        return -1;
    }

    char *buf = malloc(nbytes);
    int r;
    for (r = 0; r < nrecords; r++) {
        fread(buf, 1, nbytes, src);
        fwrite(buf, 1, nbytes, dest);
    }
    free(buf);

    if (fclose(src) != 0) {
        fprintf(stderr, "Cannot close source file\n");
        return -1;
    }

    if (fclose(dest) != 0) {
        fprintf(stderr, "Cannot close destination file\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Command not specified\n");
        return -1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "generate") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Invalid arguments\n");
            return -1;
        }
        const char* path = argv[2];
        const int nrecords = atoi(argv[3]);
        const int nbytes = atoi(argv[4]);

        srand(time(NULL));
        generate_sys(path, nrecords, nbytes); // or: generate_lib()
        return 0;
    }

    if (strcmp(cmd, "sort") == 0) {
        if (argc != 6) {
            fprintf(stderr, "Invalid arguments\n");
            return -1;
        }
        const char* path = argv[2];
        const int nrecords = atoi(argv[3]);
        const int nbytes = atoi(argv[4]);
        const char* method = argv[5];

        if (strcmp(method, "sys") == 0) {
            sort_sys(path, nrecords, nbytes);
            return 0;
        }
        if (strcmp(method, "lib") == 0) {
            sort_lib(path, nrecords, nbytes);
            return 0;
        }

        fprintf(stderr, "Invalid method\n");
        return -1;
    }

    if (strcmp(cmd, "copy") == 0) {
        if (argc != 7) {
            fprintf(stderr, "Invalid arguments\n");
            return -1;
        }
        const char* path_src = argv[2];
        const char* path_dest = argv[3];
        const int nrecords = atoi(argv[4]);
        const int nbytes = atoi(argv[5]);
        const char* method = argv[6];

        if (strcmp(method, "sys") == 0) {
            copy_sys(path_src, path_dest, nrecords, nbytes);
            return 0;
        }
        if (strcmp(method, "lib") == 0) {
            copy_lib(path_src, path_dest, nrecords, nbytes);
            return 0;
        }

        fprintf(stderr, "Invalid method\n");
        return -1;
    }
    
    fprintf(stderr, "Invalid command\n");
    return -1;
}
