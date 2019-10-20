#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#define handle_errno(msg) { perror(msg); exit(EXIT_FAILURE); }
#define test_errno(msg) { if (errno != 0) handle_errno(msg); }

#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"

static struct timeval now;

#define time_printf(fmt, ...) { \
    gettimeofday(&now, NULL); \
    printf("[%ld.%06ld] " fmt, now.tv_sec, now.tv_usec, ##__VA_ARGS__); \
}

unsigned P, T, C, N; // number of passengers, number of trolleys, capacity of trolley, number of rides

struct passenger {
    pthread_t tid; // thread id
    unsigned p; // passenger id, 0..P-1
    unsigned trolley; // trolley id
} *passengers = NULL;

struct trolley {
    pthread_t tid; // thread id
    unsigned t; // trolley id, 0..T-1
    unsigned passengers; // current number of passengers
} *trolleys = NULL;

pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_in = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_out = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_button_wait = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_button_press = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_queue = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_passengers = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_out = PTHREAD_COND_INITIALIZER;

pthread_barrier_t trolleys_barrier; // synchronizes trolleys on the track
pthread_barrier_t passengers_barrier; // makes one passenger push the start button

long current_ride_time_us; // randomized ride time
unsigned current_trolley = 0; // trolley at the station
volatile bool allow_in = false, allow_out = false; // whether passengers can get in/out
volatile bool die = false; // whether it is time for them to die

void parse_unsigned_int(const char *const str, unsigned *const uint_ptr, const char *const param_name) {
    const int n = strtol(str, NULL, 10);   
    if (n <= 0) {
        fprintf(stderr, "Invalid %s\n", param_name);
        _exit(EXIT_FAILURE);
    }
    *uint_ptr = (unsigned) n;
}

void *passenger_routine(void *args) {
    struct passenger *passenger = (struct passenger*) args;

    while (true) {

        // get in the trolley
        pthread_mutex_lock(&mutex_in);
        while (!((allow_in && trolleys[current_trolley].passengers < C) || die)) {
            pthread_cond_wait(&cond_in, &mutex_in);
        }
        {            
            if (die) {
                pthread_mutex_unlock(&mutex_in);
                break;
            }
            ++trolleys[passenger->trolley = current_trolley].passengers;
            time_printf(ANSI_COLOR_CYAN "Pasażer %u:" ANSI_COLOR_RESET " Wejście do wagonika %u, jest %u pasażerów\n",
                passenger->p, passenger->trolley, trolleys[passenger->trolley].passengers);
            pthread_cond_broadcast(&cond_in);
        }
        pthread_mutex_unlock(&mutex_in);

        // makes random passenger press the start button
        if (C > 1) {
            switch (errno = pthread_barrier_wait(&passengers_barrier)) {
                case PTHREAD_BARRIER_SERIAL_THREAD:
                    errno = 0;
                    pthread_mutex_lock(&mutex_button_wait);
                    if (die) {
                        pthread_mutex_unlock(&mutex_button_wait);
                        break;
                    }
                    pthread_mutex_unlock(&mutex_button_press);
                    time_printf(ANSI_COLOR_CYAN "Pasażer %u:" ANSI_COLOR_RESET " Naciśnięcie przycisku start\n", passenger->p);
                case 0:
                    break;
                default:
                    handle_errno("pthread_barrier_wait");
            }
        } else {
            pthread_mutex_lock(&mutex_button_wait);
            pthread_mutex_unlock(&mutex_button_press);
            time_printf(ANSI_COLOR_CYAN "Pasażer %u:" ANSI_COLOR_RESET " Naciśnięcie przycisku start\n", passenger->p);
        }

        // get out the trolley
        pthread_mutex_lock(&mutex_out);
        while (!((allow_out && passenger->trolley == current_trolley) || die)) {
            pthread_cond_wait(&cond_out, &mutex_out);
        }
        {             
            if (die) {
                pthread_mutex_unlock(&mutex_out);
                break;
            }    
            --trolleys[passenger->trolley].passengers;
            time_printf(ANSI_COLOR_CYAN "Pasażer %u:" ANSI_COLOR_RESET " Opuszczenie wagonika %u, zostało %u pasażerów\n",
                passenger->p, passenger->trolley, trolleys[passenger->trolley].passengers);
        }
        pthread_cond_broadcast(&cond_out);
        pthread_mutex_unlock(&mutex_out);
    }

    time_printf(ANSI_COLOR_CYAN "Pasażer %u:" ANSI_COLOR_RESET " Zakończenie pracy wątku (%ld)\n", passenger->p, pthread_self());
    pthread_exit(NULL);    
}

void *trolley_routine(void *args) {
    struct trolley *trolley = (struct trolley*) args;

    unsigned n = 0;
    while (true) {

        // queue trolleys
        pthread_mutex_lock(&mutex_queue);
        while (current_trolley != trolley->t) {
            pthread_cond_wait(&cond_queue, &mutex_queue);
        }
        {
            // open the door
            time_printf(ANSI_COLOR_MAGENTA "Wagonik %u:" ANSI_COLOR_RESET " Otwarcie drzwi\n", trolley->t);

            // if there was a previous ride
            if (n != 0) {

                // drop passengers                
                pthread_mutex_lock(&mutex_out);    
                allow_out = true;
                pthread_cond_broadcast(&cond_out);
                while (trolley->passengers > 0) {
                    pthread_cond_wait(&cond_out, &mutex_out);
                }
                {            
                    allow_out = false;     
                }
                pthread_mutex_unlock(&mutex_out);

            }

            // next ride
            ++n;

            // if there will be next ride
            if (n <= N) {

                // take passengers         
                pthread_mutex_lock(&mutex_in);
                allow_in = true;
                pthread_cond_broadcast(&cond_in);
                while (trolleys[current_trolley].passengers < C) {
                    pthread_cond_wait(&cond_in, &mutex_in);
                }
                allow_in = false;
                pthread_mutex_unlock(&mutex_in);

                // wait for start button press
                pthread_mutex_unlock(&mutex_button_wait);
                pthread_mutex_lock(&mutex_button_press);
            }
                
            // close the door
            time_printf(ANSI_COLOR_MAGENTA "Wagonik %u:" ANSI_COLOR_RESET " Zamknięcie drzwi\n", trolley->t);

            // get next trolley
            current_trolley = (current_trolley + 1) % T;
            pthread_cond_broadcast(&cond_queue);
        }
        pthread_mutex_unlock(&mutex_queue);

        // if there will be no next ride
        if (n > N) {
            break;
        }

        // randomize ride time
        if (trolley->t == T-1) {
            current_ride_time_us = rand() % 5000; // 0-5 ms
        }

        // synchronize trolleys so they depart simultaneously
        if (T > 1) {
            switch (errno = pthread_barrier_wait(&trolleys_barrier)) {
                case PTHREAD_BARRIER_SERIAL_THREAD:
                    errno = 0;
                    time_printf(ANSI_COLOR_GREEN "Przejazd %u:" ANSI_COLOR_RESET " Rozpoczęcie jazdy\n", n);
                case 0:
                    break;
                default:
                    handle_errno("pthread_barrier_wait");
            }
        } else {
            time_printf(ANSI_COLOR_GREEN "Przejazd %u:" ANSI_COLOR_RESET " Rozpoczęcie jazdy\n", n);
        }

        // enjoy the joyride
        usleep(current_ride_time_us);

        // synchronize trolleys so they arrive simultaneously
        if (T > 1) {
            switch (errno = pthread_barrier_wait(&trolleys_barrier)) {
                case PTHREAD_BARRIER_SERIAL_THREAD:
                    errno = 0;
                    time_printf(ANSI_COLOR_GREEN "Przejazd %u:" ANSI_COLOR_RESET " Zakończenie jazdy\n", n);
                case 0:
                    break;
                default:
                    handle_errno("pthread_barrier_wait");
            }
        } else {
            time_printf(ANSI_COLOR_GREEN "Przejazd %u:" ANSI_COLOR_RESET " Zakończenie jazdy\n", n);
        }
    }

    if (T > 1) {
        switch (errno = pthread_barrier_wait(&trolleys_barrier)) {
            case PTHREAD_BARRIER_SERIAL_THREAD:
                errno = 0;
            case 0:
                break;
            default:
                handle_errno("pthread_barrier_wait");
        }
    }
    time_printf(ANSI_COLOR_MAGENTA "Wagonik %u:" ANSI_COLOR_RESET " Zakończenie pracy wątku (%ld)\n", trolley->t, pthread_self());
    pthread_exit(NULL);
}

void cleanup(void) {
    /*
    // cancel passenger threads
    if (passengers != NULL) {
        unsigned p;
        for (p = 0; p < P; p++) {
            if (passengers[p].tid != 0) {
                pthread_cancel(passengers[p].tid);
                pthread_join(passengers[p].tid, NULL);
            }
        }
        free(passengers);
    }

    // cancel trolley threads
    if (trolleys != NULL) {
        unsigned t;
        for (t = 0; t < T; t++) {
            if (trolleys[t].tid != 0) {
                pthread_cancel(trolleys[t].tid);
                pthread_join(trolleys[t].tid, NULL);
            }
        }
        free(trolleys);
    }
    */

    // destroy mutexes
    pthread_mutex_destroy(&mutex_queue);
    pthread_mutex_destroy(&mutex_in);
    pthread_mutex_destroy(&mutex_out);
    pthread_mutex_destroy(&mutex_button_wait);
    pthread_mutex_destroy(&mutex_button_press);

    // destroy condition variables
    pthread_cond_destroy(&cond_queue);
    pthread_cond_destroy(&cond_passengers);
    pthread_cond_destroy(&cond_in);
    pthread_cond_destroy(&cond_out);

    // destroy trolleys barrier
    pthread_barrier_destroy(&trolleys_barrier);
    pthread_barrier_destroy(&passengers_barrier);
}

void exit_handler() {
    cleanup();
}

void sigint_handler() {
    putchar('\n');
    cleanup();
    _exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    // parse program arguments
    if (argc != 1+4) {
        fprintf(stderr, "Syntax: <number of passengers> <number of trolleys> <capacity of trolley> <number of rides>\n");
        _exit(EXIT_FAILURE);
    }
    parse_unsigned_int(argv[1], &P, "number of passengers");
    parse_unsigned_int(argv[2], &T, "number of trolleys");
    parse_unsigned_int(argv[3], &C, "capacity of trolley");
    parse_unsigned_int(argv[4], &N, "number of rides");

    // check program arguments
    if (P < T*C) {
        fprintf(stderr, "Too few passengers (must be at least %d*%d=%d)\n", T, C, T*C);
        _exit(EXIT_FAILURE);
    }

    // register exit handler
    if (atexit(exit_handler) != 0) {
        perror("atexit");
        return EXIT_FAILURE;
    }

    // register signal handler
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }

    // initialize mutexes
    pthread_mutex_lock(&mutex_button_wait);
    pthread_mutex_lock(&mutex_button_press);

    // initialize barriers
	errno = pthread_barrier_init(&trolleys_barrier, NULL, T);
    test_errno("pthread_barrier_init");

    errno = pthread_barrier_init(&passengers_barrier, NULL, C);
    test_errno("pthread_barrier_init");

    // create passenger array
    if ((passengers = calloc(P, sizeof(struct passenger))) == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    // create trolley array
    if ((trolleys = calloc(T, sizeof(struct trolley))) == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    // create passenger threads
    {
        unsigned p;
        for (p = 0; p < P; p++) {
            passengers[p].p = p;
            if ((errno = pthread_create(&passengers[p].tid, NULL, passenger_routine, &passengers[p])) != 0) {
                passengers[p].tid = 0;
                perror("pthread_create");
                return EXIT_FAILURE;
            }
        }
    }

    // create trolley threads
    {
        unsigned t;
        for (t = 0; t < T; t++) {
            trolleys[t].t = t;
            trolleys[t].passengers = 0;
            if ((errno = pthread_create(&trolleys[t].tid, NULL, trolley_routine, &trolleys[t])) != 0) {
                trolleys[t].tid = 0;
                perror("pthread_create");
                return EXIT_FAILURE;
            }
        }
    }

    // woo-hoo!

    // wait for trolley threads
    {
        unsigned t;
        for (t = 0; t < T; t++) {
            if ((errno = pthread_join(trolleys[t].tid, NULL)) != 0) {
                perror("pthread_join");
                return EXIT_FAILURE;
            }
            trolleys[t].tid = 0;
        }
    }

    // kill passengers
    die = true;
    pthread_cond_broadcast(&cond_in);
    pthread_cond_broadcast(&cond_out);
    pthread_mutex_unlock(&mutex_button_wait);

    // wait for passenger threads
    {
        unsigned p;
        for (p = 0; p < P; p++) {
            if ((errno = pthread_join(passengers[p].tid, NULL)) != 0) {
                perror("pthread_join");
                return EXIT_FAILURE;
            }
            passengers[p].tid = 0;
        }
    }

    // cleanup (will be executed automatically)

    return EXIT_SUCCESS;
}