#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

static inline int randint(int a, int b) {
    return rand() % (b - a + 1) + a;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <fifo path> <n>", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *fifo_path = argv[1];
    if (mkfifo(fifo_path, S_IRUSR | S_IWUSR) != 0 && errno != EEXIST) {
        perror("Cannot make fifo");
        fprintf(stderr, "Please try again in /tmp directory\n");
        exit(EXIT_FAILURE);
    }

    int n = strtol(argv[2], NULL, 10);

    pid_t pid = getpid();
    printf("Slave PID: %d\n", pid);

    srand(time(NULL) + getpid());

    /*
    FILE *fifo = fopen(fifo_path, "w");
    if (fifo == NULL) {
    */
    int fifo = open(fifo_path, O_WRONLY);
    if (fifo == -1) {
        perror("Cannot open fifo");
        exit(EXIT_FAILURE);
    }

    while (n--) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "%d\t", pid);

        FILE* date_output = popen("date", "r");
        fgets(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), date_output);
        pclose(date_output);

        /*
        fputs(buffer, fifo); 
        fwrite(buffer, 1, strlen(buffer), fifo);
        // fputs and fwrite flush fifo only on close (tested both on Ubuntu WSL and jagular.iisg.agh.edu.pl)
        */
        write(fifo, buffer, strlen(buffer)); // it works!

        printf("Writing to fifo...\n");

        sleep(randint(2, 5));
    }

    /*
    if (fclose(fifo) != 0) {
    */
    if (close(fifo) != 0) {
        perror("Cannot close fifo");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
