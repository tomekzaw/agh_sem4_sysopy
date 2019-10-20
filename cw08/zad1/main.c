#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))
#define clip(x,a,b) min(max((x),(a)),(b))

int nthreads; // number of threads

enum Mode {MODE_BLOCK, MODE_INTERLEAVED} mode;

struct image {
    unsigned W, H;
    uint8_t **pixels;
} *I = NULL, *J = NULL; // input, output

struct filter {
    unsigned c;
    double **pixels;
} *K = NULL; // filter

struct thread { // thread arguments
    pthread_t tid;
    struct image *I, *J;
    struct filter *K;
    unsigned x_min, x_max, x_step;
    long time_diff_usec;
} *threads = NULL; // array of filter arguments

FILE *fp = NULL; // for loading and saving images

long timeval_diff_usec(const struct timeval *start, const struct timeval *end) { // calculate time difference in microseconds
    return (end->tv_sec - start->tv_sec) * 1e6L + (end->tv_usec - start->tv_usec);
}

struct image *image_alloc(const unsigned W, const unsigned H) {
    // allocate struct image
    struct image *const image = malloc(sizeof(struct image));
    if (image == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // set image width and height
    image->W = W;
    image->H = H;

    // allocate 1-D array of pixels
    uint8_t *const data = calloc(image->W * image->H, sizeof(uint8_t));
    if (data == NULL) {
        free(image);
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    // allocate 1-D array of column pointers
    if ((image->pixels = calloc(image->W, sizeof(uint8_t*))) == NULL) {
        free(image);
        free(data);
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    // set column pointers
    unsigned x;
    for (x = 0; x < image->W; ++x) {
        image->pixels[x] = data + x * image->H; // => data == *image->pixels
    }

    return image;
}

void image_free(struct image *image) {
    if (image->pixels != NULL) {
        free(*image->pixels); // free column pointers
        free(image->pixels); // free data
        image->pixels = NULL;
    }
    free(image);
}

struct image *image_load(FILE *const fp) {
    // read image header
    fscanf(fp, "P2 ");
    while (fscanf(fp, "#%*s%*[^\r\n]c")); // skip comments

    // read image size
    unsigned W, H;
    if (fscanf(fp, "%u %u 255", &W, &H) != 2) {
        fprintf(stderr, "Image header corrupted\n");
        exit(EXIT_FAILURE);
    }

    // allocate image
    struct image *const image = image_alloc(W, H);

    // read image pixel values
    unsigned x, y;
    for (y = 0; y < image->H; ++y) {
        for (x = 0; x < image->W; ++x) {
            if (fscanf(fp, " %hhu", &image->pixels[x][y]) != 1) {
                image_free(image);
                fprintf(stderr, "Image corrupted\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    return image;
}

void image_save(struct image *const image, FILE *const fp) {
    // write image header
    fprintf(fp, "P2\n%u %u\n255", image->W, image->H);

    // write image pixel values
    unsigned x, y;
    for (y = 0; y < image->H; ++y) {
        fputc('\n', fp);
        for (x = 0; x < image->W; ++x) {
            fprintf(fp, "%u ", image->pixels[x][y]);
        }
    }
}

struct filter *filter_alloc(const unsigned c) {
    // allocate struct filter
    struct filter *const filter = malloc(sizeof(struct filter));
    if (filter == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // set filter size
    filter->c = c;

    // allocate 1-D array of pixels
    double *const data = calloc(filter->c * filter->c, sizeof(double));
    if (data == NULL) {
        free(filter);
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    // allocate 1-D array of column pointers
    if ((filter->pixels = calloc(filter->c, sizeof(double*))) == NULL) {
        free(filter);
        free(data);
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    // set column pointers
    unsigned x;
    for (x = 0; x < filter->c; ++x) {
        filter->pixels[x] = data + x * filter->c; // => data == *image->pixels
    }

    return filter;
}

void filter_free(struct filter *filter) {
    if (filter->pixels != NULL) {
        free(*filter->pixels); // free column pointers
        free(filter->pixels); // free data
        filter->pixels = NULL;
    }
    free(filter);
}

struct filter *filter_load(FILE *const fp) {
    // read filter header
    unsigned c;
    if (fscanf(fp, "%u", &c) != 1) {
        fprintf(stderr, "Filter header corrupted\n");
        exit(EXIT_FAILURE);
    }

    // allocate filter
    struct filter *const filter = filter_alloc(c);

    // read filter pixel values
    unsigned x, y;
    for (y = 0; y < filter->c; ++y) {
        for (x = 0; x < filter->c; ++x) {
            if (fscanf(fp, " %lg", &filter->pixels[x][y]) != 1) {
                filter_free(filter);
                fprintf(stderr, "Filter corrupted\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    return filter;
}

void filter_save(struct filter *const filter, FILE *const fp) {
    // write filter header
    fprintf(fp, "%u", filter->c);

    // write filter pixel values
    unsigned x, y;
    for (y = 0; y < filter->c; ++y) {
        fputc('\n', fp);
        for (x = 0; x < filter->c; ++x) {
            fprintf(fp, "%lg ", filter->pixels[x][y]);
        }
    }
}

double array_sum_kahan(const double *const array, const size_t length) {
    // Kahan summation algorithm
    double sum = 0.0, err = 0.0;
    unsigned i;
    for (i = 0; i < length; ++i) {
        double p = array[i] - err;
        double t = sum + p;
        err = (t - sum) - p;
        sum = t;
    }
    return sum;
}

static inline double filter_calculate_sum(const struct filter *const filter) {
    // calculate sum of pixel values
    return array_sum_kahan(*filter->pixels, filter->c * filter->c);
}
   
void filter_normalize(const struct filter *const filter) { // normalize filter so that sum of pixels equals 1
    // calculate filter sum
    double sum = filter_calculate_sum(filter);
    if (sum == 0) {
        return; // don't normalize if sum equals 0
    }

    // divide each pixel value by sum
    unsigned x, y;
    for (x = 0; x < filter->c; ++x) {
        for (y = 0; y < filter->c; ++y) {
            filter->pixels[x][y] /= sum;
        }
    }
}

inline static void filter_memcpy(const struct filter *const dest, const struct filter *const src) {
    // copy filter data
    memcpy(*dest->pixels, *src->pixels, src->c * src->c * sizeof(double));
}

static void filter_calculate(const struct image *const I, const struct filter *const K, const struct image *const J,
                                const unsigned x_min, const unsigned x_max, const unsigned x_step, long *const time_diff_usec) {
    // allocate helper filter (Kahan summation algorithm has already been implemented for filter sum)
    struct filter *const H = filter_alloc(K->c);

    // calculate this only once
    const int ceil_c_2 = ceil(K->c / 2.0);

    // start time measurement
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);

    // calculate part of output image
    unsigned x, y;
    for (x = x_min; x <= min(I->W-1, x_max); x += x_step) {
        for (y = 0; y < I->H; ++y) {    
            /* J[x][y] is calculated using Kahan summation algorithm
               by multiplying filter pixel values by corresponding image pixel values
               and calculating sum of helper filter */

            // initialize helper filter values with real filter values
            filter_memcpy(H, K);

            // multiply each filter pixel value by corresponding image pixel value
            unsigned i, j;
            for (i = 0; i < K->c; ++i) {
                for (j = 0; j < K->c; ++j) {
                    H->pixels[i][j] *= I->pixels[clip(x-ceil_c_2+i, 0, I->W-1)][clip(y-ceil_c_2+j, 0, I->H-1)];
                }
            }

            // calculate helper filter sum, round and clip result and store in output image
            J->pixels[x][y] = clip(round(filter_calculate_sum(H)), 0, 255);
        }
    }

    // end time measurement
    gettimeofday(&time_end, NULL);
    *time_diff_usec = timeval_diff_usec(&time_start, &time_end);
    
    // free helper filter
    filter_free(H);
}

struct image *image_apply_filter_single_thread(const struct image *const I, const struct filter *const K) {
    // allocate output image
    struct image *const J = image_alloc(I->W, I->H);

    // calculate part of output image
    long time_diff_usec;
    filter_calculate(I, K, J, 0, I->W-1, 1, &time_diff_usec);

    // display calculation time
    printf("Single thread: %ld us\n", time_diff_usec);

    // return output image
    return J;
}

static void *thread_routine(void *const args) {
    // get thread routine arguments
    struct thread *const thread = (struct thread*) args;

    // calculate part of output image
    filter_calculate(thread->I, thread->K, thread->J, thread->x_min, thread->x_max, thread->x_step, &thread->time_diff_usec);

    // return calculation time
    pthread_exit(&thread->time_diff_usec); // or: return &thread->time_diff_usec;
}

void image_apply_filter_multi_thread(struct image *const I, struct filter *const K, struct image **const Jptr,
                                        const unsigned m, struct thread **const threadsptr) {
    // allocate output image
    struct image *const J = *Jptr = image_alloc(I->W, I->H);

    // allocate thread args array
    struct thread *const threads = *threadsptr = calloc(nthreads, sizeof(struct thread));

    // start time measurement
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);
   
    // run threads
    unsigned k;
    for (k = 0; k < m; ++k) {
        struct thread *const thread = &threads[k];
        thread->I = I;
        thread->J = J;
        thread->K = K;

        switch (mode) {
            case MODE_BLOCK:
                thread->x_min = k * ceil(I->W / (float) m);
                thread->x_max = (k + 1) * ceil(I->W / (float) m) - 1;
                thread->x_step = 1;
                break;

            case MODE_INTERLEAVED:
                thread->x_min = k;
                thread->x_max = I->W - 1;
                thread->x_step = m;
                break;

            default:
                fprintf(stderr, "Invalid mode\n");
                exit(EXIT_FAILURE);
        }

        if (pthread_create(&thread->tid, NULL, thread_routine, &threads[k]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // wait for threads
    for (k = 0; k < m; ++k) {
        long *const time_diff_usec;
        if (pthread_join(threads[k].tid, (void**) &time_diff_usec) != 0) {
            exit(EXIT_FAILURE);
            perror("pthread_join");
        }
        printf("Thread %ld: %ld us\n", threads[k].tid, *time_diff_usec);
    }

    // end time measurement
    gettimeofday(&time_end, NULL);
    long time_diff_usec = timeval_diff_usec(&time_start, &time_end);
    printf("Main thread: %ld us\n", time_diff_usec);

    // when completed, output image is pointed by Jptr
}

void cleanup(void) {
    // free input image
    if (I != NULL) {
        image_free(I);
    }
    
    // free output image
    if (J != NULL) {
        image_free(J);
    }

    // free filter
    if (K != NULL) {
        filter_free(K);
    }

    // free thread args array
    if (threads != NULL) {
        free(threads);
    }

    // close file if any is open
    if (fp != NULL) {
        if (fclose(fp) != 0) {
            perror("fclose");
        }
    }
}

int main(int argc, char *argv[]) {

    // register exit handler
    if (atexit(cleanup) != 0) {
        perror("atexit");
        return EXIT_FAILURE;
    }

    /*
    // generate filters
    const unsigned cs[] = {3, 5, 8, 16, 32, 65};
    unsigned i;
    for (i = 0; i < sizeof(cs) / sizeof(*cs); i++) {
        const unsigned c = cs[i];

        // create random filter
        K = filter_alloc(c);
        unsigned x, y;
        for (x = 0; x < c; ++x) {
            for (y = 0; y < c; ++y) {
                K->pixels[x][y] = rand() / (float) RAND_MAX;
            }
        }
        filter_normalize(K);

        // save filter to file
        char path[19+1];
        snprintf(path, sizeof(path), "filters/rand_%u.txt", c);
        if ((fp = fopen(path, "w")) == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        filter_save(K, fp);
        if (fclose(fp) != 0) {
            perror("fclose");
            exit(EXIT_FAILURE);
        }

        // free filter
        filter_free(K);
        K = NULL;
    }
    return EXIT_SUCCESS;
    */

    // parse program arguments
    if (argc != 6) {
        fprintf(stderr, "Syntax: <number of threads> <mode> <input> <filter> <output>\n");
        return EXIT_FAILURE;
    }

    // parse number of threads
    if ((nthreads = strtol(argv[1], NULL, 10)) < 0) {
        fprintf(stderr, "Invalid number of threads\n");
        return EXIT_FAILURE;
    }

    // parse filtering mode
    if (strcmp(argv[2], "block") == 0) {
        mode = MODE_BLOCK;
    } else if (strcmp(argv[2], "interleaved") == 0) {
        mode = MODE_INTERLEAVED;
    } else {
        fprintf(stderr, "Invalid mode\n");
        return EXIT_FAILURE;
    }

    // load input image from file
    if ((fp = fopen(argv[3], "r")) == NULL) {
        if (errno == ENOENT) {
            fprintf(stderr, "File %s not found\n", argv[3]);
        }
        perror("fopen");
        return EXIT_FAILURE;
    }
    
    I = image_load(fp);

    if (fclose(fp) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }
    fp = NULL;

    // load filter from file
    if ((fp = fopen(argv[4], "r")) == NULL) {
        if (errno == ENOENT) {
            fprintf(stderr, "File %s not found\n", argv[4]);
        }
        perror("fopen");
        return EXIT_FAILURE;
    }

    K = filter_load(fp);

    if (fclose(fp) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }
    fp = NULL;

    // normalize filter
    // filter_normalize(K);

    // apply filter
    if (nthreads == 0) {
        J = image_apply_filter_single_thread(I, K);
    } else {
        image_apply_filter_multi_thread(I, K, &J, nthreads, &threads);
        free(threads);
        threads = NULL;
    }

    // free input image
    image_free(I);    
    I = NULL;

    // free filter
    filter_free(K);
    K = NULL;

    // save output image to file
    if (strlen(argv[5]) > 0) {
        if ((fp = fopen(argv[5], "w")) == NULL) {
            perror("fopen");
            return EXIT_FAILURE;
        }

        image_save(J, fp);

        if (fclose(fp) != 0) {
            perror("fclose");
            return EXIT_FAILURE;
        }
        fp = NULL;
    }

    // free output image
    image_free(J);
    J = NULL;

    return EXIT_SUCCESS;
}