#include "common.h"

unsigned int X, K, M;
int semid = -1, shmid = -1;
struct conveyor *conveyor = NULL;
unsigned int truck_total_weight_of_packages = -1;

long timeval_diff_usec(struct timeval then) {
    static struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - then.tv_sec) * 1000000 + (now.tv_usec - then.tv_usec);
}

void finish() {

    // take semaphore
    struct sembuf sop;
    sop.sem_num = 0;
    sop.sem_op = -1;
    sop.sem_flg = IPC_NOWAIT;
    semop(semid, &sop, 1);

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
        if (shmdt(conveyor) == -1) {
            perror("shmdt");
        } else {
            printf("Successfully detached shared memory\n");
        }
    }
    
    // remove shared memory
    if (shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl");
        } else {
            printf("Successfully removed shared memory\n");
        }
    }

    // remove semaphore
    if (semid != -1) {
        if (semctl(semid, 0, IPC_RMID, 0) == -1) {
            perror("semctl");
        } else {
            printf("Successfully removed semaphore\n");
        }
    }

}

void exit_handler() {
    cleanup();
}

void sigint_handler() {
    printf("\nReceived SIGINT\n");
    if (semid != -1 && shmid != -1) {
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

    // generate key
    key_t key;
    if ((key = KEY()) == -1) {
        perror("ftok");
        return EXIT_FAILURE;        
    }
    printf("key=%d\n", key);

    // create semaphore and shared memory
    size_t shm_size = sizeof(struct conveyor) + K * sizeof(struct package);
    if ((semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("semget");
    }
    if ((shmid = shmget(key, shm_size, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("shmget");
    }
    if (semid == -1 || shmid == -1) {
        if (errno == EEXIST) {
            printf("\nPerhaps trucker is already running.\
                \nPlease execute the following commands:\n");
            if (semid == -1) {
                printf("> ipcrm -S %d\n", key);
            }
            if (shmid == -1) {
                printf("> ipcrm -M %d\n", key);
            }
            printf("Thank you for your cooperation!\n\n");
        }
        return EXIT_FAILURE;
    }
    printf("semid=%d\n", semid);
    printf("shmid=%d\n", shmid);

    // initialize semaphore    
    union semun arg;    
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    // attach shared memory
    if ((conveyor = shmat(shmid, NULL, 0)) == (void*) -1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    // conveyor belt initialization
    conveyor->K = K;
    conveyor->M = M;
    conveyor->head = conveyor->tail = 0;
    conveyor->current_total_number_of_packages = 0;
    conveyor->current_total_weight_of_packages = 0;

    // prepare sembuf
    struct sembuf sop;
    sop.sem_num = 0;
    sop.sem_flg = 0;

    // trucker main loop
    truck_total_weight_of_packages = 0;
    printf("\nEmpty truck arrived, waiting for packages\n");
    while (1) {

        // take semaphore
        sop.sem_op = -1;
        if (semop(semid, &sop, 1) == -1) {
            perror("semop");
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
        sop.sem_op = 1;
        if (semop(semid, &sop, 1) == -1) {
            perror("semop");
            exit(EXIT_FAILURE);
        }

        #ifdef SLOW_MOTION
            usleep(5e5);
        #endif
    }
    pause();

    return EXIT_SUCCESS; // to avoid Wreturn-type

}
