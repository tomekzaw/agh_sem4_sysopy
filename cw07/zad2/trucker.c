#include "common.h"

unsigned int X, K, M;
sem_t *sem = SEM_FAILED;
int shmid = -1;
off_t shm_size;
struct conveyor *conveyor = NULL;
unsigned int truck_total_weight_of_packages = -1;

long timeval_diff_usec(struct timeval then) {
    static struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - then.tv_sec) * 1000000 + (now.tv_usec - then.tv_usec);
}

void finish() {

    // take semaphore
    sem_trywait(sem);

    // for each package
    unsigned current_total_number_of_packages = conveyor->current_total_number_of_packages;
    conveyor->current_total_number_of_packages = conveyor->K;
    while (current_total_number_of_packages > 0) {
        
        // examine first package in queue
        struct package *package = conveyor->packages + conveyor->head;

        // check if package can be loaded to truck
        if (truck_total_weight_of_packages + package->weight <= X) {

            // load package to truck
            conveyor->head = (conveyor->head + 1) % conveyor->K;
            current_total_number_of_packages--;
            truck_total_weight_of_packages += package->weight;
        
            long diff_usec = timeval_diff_usec(package->load_time);
            printf("loader_pid=%-7d time_diff=%ld.%06ld\tweight=%-5d truck=%d/%d\n", package->loader_pid,
                diff_usec / 1000000, diff_usec % 1000000, package->weight, truck_total_weight_of_packages, X);

        } else {
            
            // check if truck is not empty
            if (truck_total_weight_of_packages > 0) {

                // next truck
                printf("Package too heavy, full truck departed\n");
                truck_total_weight_of_packages = 0;
                #ifdef SLOW_MOTION
                    printf("Waiting for empty truck");
                    unsigned i;
                    for (i = 0; i < 3; ++i) {
                        putchar('.');
                        fflush(stdout);
                        usleep(2e5);
                    }
                    putchar('\n');
                #endif
                printf("Empty truck arrived, waiting for packages\n");

            } else {

                // next package is too heavy for truck!
                printf("Cannot load package (%d kg) from conveyor to truck (max %d kg)\n", package->weight, X);
                exit(EXIT_FAILURE);

            }
        }        

        #ifdef SLOW_MOTION
            usleep(5e5);
        #endif
    }

}

void cleanup() {

    // detach shared memory
    if (conveyor != NULL) {
        if (munmap(conveyor, shm_size) == -1) {
            perror("munmap");
        } else {
            printf("Successfully detached shared memory\n");
        }
    }
    
    // remove shared memory
    if (shmid != -1) {
        if (shm_unlink(SHARED_MEMORY_NAME) == -1) {
            perror("shm_unlink");
        } else {
            printf("Successfully removed shared memory\n");
        }
    }

    // close semaphore
    if (sem != SEM_FAILED) {
        if (sem_close(sem) == -1) {
            perror("semctl");
        } else {
            printf("Successfully closed semaphore\n");
        }

        // remove semaphore
        if (sem_unlink(SEMAPHORE_NAME) == -1) {
            perror("sem_unlink");
        } else {
            printf("Successfully unlinked semaphore\n");
        }
    }

}

void exit_handler() {
    cleanup();
}

void sigint_handler() {
    printf("\nReceived SIGINT\n");
    if (sem != SEM_FAILED && shmid != -1) {
        finish();
    }
    cleanup();
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    // parse program arguments
    if (argc != 4) {
        fprintf(stderr, "Syntax: %s X K M\n\
        X - truck capacity (kg)\n\
        K - max number packages on conveyor belt\n\
        M - conveyor belt capacity (kg)\n", argv[0]);
        return EXIT_FAILURE;
    }
    parse_unsigned_int(argv[1], &X, "X");
    parse_unsigned_int(argv[2], &K, "K");
    parse_unsigned_int(argv[3], &M, "M");

    // display program arguments
    printf("X=%d, K=%d, M=%d\n", X, K, M);
    printf("trucker_pid=%d\n", getpid());

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

    // create and initialize semaphore and shared memory
    if ((sem = sem_open(SEMAPHORE_NAME, O_RDWR | O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
        perror("sem_open");
    }
    if ((shmid = shm_open(SHARED_MEMORY_NAME, O_RDWR | O_CREAT | O_EXCL, 0666)) == -1) {
        perror("shm_open");
    }
    if (sem == SEM_FAILED || shmid == -1) {
        if (errno == EEXIST) {
            printf("\nPerhaps trucker is already running.\
                \nPlease execute the following commands:\n");
            if (sem == SEM_FAILED) {
                printf("> rm /dev/shm/sem.%s\n", SEMAPHORE_NAME+1);
            }
            if (shmid == -1) {
                printf("> rm /dev/shm/%s\n", SHARED_MEMORY_NAME+1);
            }
            printf("Thank you for your cooperation!\n\n");
        }
        return EXIT_FAILURE;
    }
    shm_size = sizeof(struct conveyor) + K * sizeof(struct package);
    if (ftruncate(shmid, shm_size) == -1) {
        perror("ftruncate");
        return EXIT_FAILURE;
    }
    printf("sem=%p\n", sem);
    printf("shmid=%d\n", shmid);

    // attach shared memory
    if ((conveyor = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0)) == (void*) -1) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    conveyor->shm_size = shm_size;

    // conveyor belt initialization
    conveyor->K = K;
    conveyor->M = M;
    conveyor->head = conveyor->tail = 0;
    conveyor->current_total_number_of_packages = 0;
    conveyor->current_total_weight_of_packages = 0;

    // trucker main loop
    truck_total_weight_of_packages = 0;
    printf("\nEmpty truck arrived, waiting for packages\n");
    while (1) {

        // take semaphore
        if (sem_wait(sem) == -1) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        // check if there are packages in queue
        if (conveyor->current_total_number_of_packages > 0) {
            
            // examine first package in queue
            struct package *package = conveyor->packages + conveyor->head;

            // check if package can be loaded to truck
            if (truck_total_weight_of_packages + package->weight <= X) {

                // load package to truck
                conveyor->head = (conveyor->head + 1) % conveyor->K;
                conveyor->current_total_number_of_packages--;
                conveyor->current_total_weight_of_packages -= package->weight;
                truck_total_weight_of_packages += package->weight;
          
                long diff_usec = timeval_diff_usec(package->load_time);
                printf("loader_pid=%-7d time_diff=%ld.%06ld\tweight=%-5d truck=%d/%d\n", package->loader_pid,
                    diff_usec / 1000000, diff_usec % 1000000, package->weight, truck_total_weight_of_packages, X);

            } else {
                
                // check if truck is not empty
                if (truck_total_weight_of_packages > 0) {

                    // next truck
                    printf("Package too heavy, full truck departed\n");
                    truck_total_weight_of_packages = 0;
                    #ifdef SLOW_MOTION
                        printf("Waiting for empty truck");
                        unsigned i;
                        for (i = 0; i < 3; ++i) {
                            putchar('.');
                            fflush(stdout);
                            usleep(2e5);
                        }
                        putchar('\n');
                    #endif
                    printf("Empty truck arrived, waiting for packages\n");

                } else {

                    // next package is too heavy for truck!
                    printf("Cannot load package (%d kg) from conveyor to truck (max %d kg)\n", package->weight, X);
                    exit(EXIT_FAILURE);

                }
            }
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
    pause();

    return EXIT_SUCCESS; // to avoid Wreturn-type

}
