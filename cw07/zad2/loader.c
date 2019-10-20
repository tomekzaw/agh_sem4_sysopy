#include "common.h"

pid_t loader_pid;
sem_t *sem = SEM_FAILED;
int shmid = -1;
off_t shm_size;
struct conveyor *conveyor = NULL;

void cleanup() {
    
    // detach shared memory
    if (conveyor != NULL) {
        if (munmap(conveyor, shm_size) == -1) {
            perror("munmap");
        } else {
            printf("(loader_pid=%d) Successfully detached shared memory\n", loader_pid);
        }
    }
}

void exit_handler() {
    cleanup();
}

void sigint_handler() {
    printf("\nReceived SIGINT\n");
    cleanup();
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    // parse program arguments
    if (!(argc == 2 || argc == 3)) {
        fprintf(stderr, "Syntax: %s N [C]\n\
        N - max weight of package\n\
        C - number of packages to load (optional)\n", argv[0]);
        return EXIT_FAILURE;
    }
    unsigned int N, C = 0;
    parse_unsigned_int(argv[1], &N, "N");
    if (argc == 3) {
        parse_unsigned_int(argv[2], &C, "C");
    }

    // display program arguments
    printf("N=%d", N);
    if (C != 0) {
        printf(", C=%d", C);
    }

    // get loader pid
    printf("\nloader_pid=%d\n", loader_pid = getpid());

    // register exit handler
    if (atexit(exit_handler) == -1) {
        perror("atexit");
        return EXIT_FAILURE;
    }

    // register SIGINT handler (Ctrl+C)
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    // open semaphore and shared memory
    if ((sem = sem_open(SEMAPHORE_NAME, O_RDWR)) == SEM_FAILED) {
        perror("sem_open");
    }
    if ((shmid = shm_open(SHARED_MEMORY_NAME, O_RDWR, 0)) == -1) {
        perror("shm_open");
    }
    if (sem == SEM_FAILED || shmid == -1) {
        if (errno == ENOENT) {
            printf("\nPerhaps trucker is not running\
                \nPlease execute the following command:\
                \n> ./trucker X K M\n\n");
        }
        return EXIT_FAILURE;
    }
    printf("sem=%p\n", sem);
    printf("shmid=%d\n", shmid);

    // attach shared memory
    off_t *shm_size_tmp;
    if ((shm_size_tmp = mmap(NULL, sizeof(off_t), PROT_READ, MAP_SHARED, shmid, 0)) == (void*) -1) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    shm_size = *shm_size_tmp;
    if (munmap(shm_size_tmp, 2*sizeof(off_t)) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }  
    if ((conveyor = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0)) == (void*) -1) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    // initialize loader    
    srand(time(NULL));
    printf("\nLoading packages...\nIf nothing happens, please restart trucker and try again.\n");

    // loader main loop    
    unsigned int i;
    for (i = 0; C == 0 || i < C; ++i) {

        // get random package weight
        unsigned weight = rand() % N + 1; // 1..N

        // load package
        int try = 1;
        while (try) {

            // take semaphore
            if (sem_wait(sem) == -1) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if (conveyor->current_total_number_of_packages < conveyor->K && conveyor->current_total_weight_of_packages + weight <= conveyor->M) {
                struct package *package = conveyor->packages + conveyor->tail;
                package->weight = weight;
                package->loader_pid = loader_pid;
                gettimeofday(&package->load_time, NULL);
                conveyor->current_total_weight_of_packages += weight;
                conveyor->current_total_number_of_packages++;
                conveyor->tail = (conveyor->tail + 1) % conveyor->K; // commit happens here
                try = 0;
                printf("time=%ld.%06ld    loader_pid=%-7dweight=%-4d %4d/%d%7d/%d\n",
                    package->load_time.tv_sec, package->load_time.tv_usec, loader_pid, package->weight,
                    conveyor->current_total_number_of_packages, conveyor->K, conveyor->current_total_weight_of_packages, conveyor->M);
            }

            // give semaphore
            if (sem_post(sem) == -1) {
                perror("sem_post");
                exit(EXIT_FAILURE);
            }

            #ifdef SLOW_MOTION
                usleep(5e5);
            #endif
        }
    }

    printf("(loader_pid=%d) Successfully loaded %d package%s\n", loader_pid, C, C > 1 ? "s" : "");
    return EXIT_SUCCESS; // to avoid Wreturn-type
}
