#include "common.h"

int main(int argc, char *argv[]) {

    // parse program arguments
    if (argc != 2) {
        fprintf(stderr, "Syntax: %s number_of_loaders\n", argv[0]);
        return EXIT_FAILURE;
    }
    unsigned int number_of_workers;
    parse_unsigned_int(argv[1], &number_of_workers, "number_of_workers");

    // spawn loaders
    unsigned i;
    for (i = 0; i < number_of_workers; ++i) {

        // spawn one loader
        switch (vfork()) {
            case -1: // fork failed
                perror("fork");
                exit(EXIT_FAILURE);

            case 0: // child process
                execl("./loader", "./loader", "10", NULL);
                perror("exec");
                exit(EXIT_FAILURE);
        }

    }

    // wait for all children
    while (wait(NULL) != -1);
    return EXIT_SUCCESS;
    
}