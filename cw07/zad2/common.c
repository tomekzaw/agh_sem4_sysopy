#include "common.h"

void parse_unsigned_int(char *str, unsigned int *uint_ptr, const char *param_name) {
    int n = strtol(str, NULL, 10);   
    if (n <= 0) {
        fprintf(stderr, "Invalid parameter %s\n", param_name);
        exit(EXIT_FAILURE);
    }
    *uint_ptr = n;
}
