#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define UNLINK_FIFO_AFTER

char *fifo_path;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fifo path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fifo_path = argv[1];
    if (mkfifo(fifo_path, S_IRUSR | S_IWUSR) != 0 && errno != EEXIST) {
        perror("Cannot make fifo");
        fprintf(stderr, "Please try again in /tmp directory\n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for slaves...\n");

    /*
    FILE *fp = fopen(fifo_path, "r");
    if (fp == NULL) {
    */
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("Cannot open fifo");
        exit(EXIT_FAILURE);
    }

    /*
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        fwrite(buffer, 1, strlen(buffer), stdout);
    }
    */
    char c;
    while (read(fd, &c, 1) == 1) {
        write(STDOUT_FILENO, &c, 1);
    }

    /*
    if (fclose(fp) != 0) {
    */
    if (close(fd) != 0) {
        perror("Cannot close fifo");
        exit(EXIT_FAILURE);        
    }

    #ifdef UNLINK_FIFO_AFTER
        if (unlink(fifo_path) != 0) {
            perror("Cannot unlink fifo");
            exit(EXIT_FAILURE);        
        }
    #endif

    exit(EXIT_SUCCESS);
}
