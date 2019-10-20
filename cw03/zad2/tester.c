#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

char random_byte(void) {
    return rand() % ('z' - 'a' + 1) + 'a';
}

char* random_bytes(size_t nbytes) {
    char *bytes = malloc(nbytes+1);
    char *p = bytes+nbytes;
    *p = '\0';
    while (p-- != bytes) *p = random_byte();
    return bytes;
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <plik> <pmin> <pmax> <bytes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *rpath = realpath(argv[1], NULL);
    if (rpath == NULL) {
        fprintf(stderr, "File %s not found\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    const int pmin = atoi(argv[2]);
    if (pmin < 0) {
        fprintf(stderr, "pmin must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    const int pmax = atoi(argv[3]);
    if (pmax <= 0) {
        fprintf(stderr, "pmax must be a positive integer\n");
        exit(EXIT_FAILURE);
    }
    if (pmin > pmax) {
        fprintf(stderr, "pmin must be a less than or equal pmax\n");
        exit(EXIT_FAILURE);        
    }

    const size_t nbytes = atoi(argv[4]);
    if (pmin <= 0) {
        fprintf(stderr, "bytes must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    while (1) { // infinite loop
        unsigned seconds = rand() % (pmax - pmin + 1) + pmin;
        sleep(seconds);

        FILE *fp = fopen(rpath, "a");
        if (fp == NULL) {
            fprintf(stderr, "Cannot open file %s\n", rpath);
            exit(EXIT_FAILURE);
        }

        char datetime[20]; // strlen("YYYY-MM-DD HH:MM:SS")+1=20      
        time_t now = time(NULL);  
        strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
        char *bytes = random_bytes(nbytes);
        char *buffer = malloc(64+nbytes); // 1+strlen(getpid())+strlen(seconds)+strlen(datetime)<=64
        sprintf(buffer, "\n%u %u %s %s", getpid(), seconds, datetime, bytes);
        free(bytes);

        fwrite(buffer, 1, strlen(buffer), fp);
        free(buffer);
        fclose(fp);
    }
}
