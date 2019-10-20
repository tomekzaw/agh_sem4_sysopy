#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// todo: struct?
static char** table = NULL;
static int table_size, next_index;
static char *curr_dir = NULL, *curr_file = NULL, *curr_temp_file = NULL;

void create_table(int size) {
    if (table) {
        fprintf(stderr, "Table already created\n");
        exit(1);
    }
    table_size = size;
    table = calloc(table_size, sizeof(char*));
    next_index = 0;
}

void set_current_directory(char* dir) {
    if (curr_dir) {
        free(curr_dir);
    }
    curr_dir = strdup(dir);
}

void set_current_file(char* file) {
    if (curr_file) {
        free(curr_file);
    }
    curr_file = strdup(file);
}

void set_current_temp_file(char* temp_file) {
    if (curr_temp_file) {
        free(curr_temp_file);
    }
    curr_temp_file = strdup(temp_file);
}

void search_directory(void) {
    if (!table) {
        fprintf(stderr, "Table not created\n");
        exit(2);
    }
    if (!curr_dir) {
        fprintf(stderr, "Directory not specified\n");
        exit(3);
    }
    if (!curr_file) {
        fprintf(stderr, "File not specified\n");
        exit(4);
    }
    if (!curr_temp_file) {
        fprintf(stderr, "Temporary file not specified\n");
        exit(5);
    }
    int length = strlen("find  -name  >  2>/dev/null") + strlen(curr_dir) + strlen(curr_file) + strlen(curr_temp_file);
    char* cmd = calloc(length+1, sizeof(char));
    sprintf(cmd, "find %s -name %s > %s 2>/dev/null", curr_dir, curr_file, curr_temp_file);
    system(cmd);
}

int create_block(void) {
    if (next_index == table_size) {
        fprintf(stderr, "Table overflow\n");
        exit(6);
    }
    int index = next_index++;

    /*
    FILE* fp = fopen(curr_temp_file, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open temporary file\n");
        exit(6);
    }
    unlink(curr_temp_file);

    fseek(fp, 0L, SEEK_END);
    int length = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
        
    char* buffer = calloc(length, sizeof(char)); // sizeof(char) = 1
    if (!buffer) {
        fprintf(stderr, "Cannot allocate block\n");
        exit(7);
    }
    
    if (fread(buffer, sizeof(char), length, fp) != length) {
        fprintf(stderr, "Cannot read temporary file\n");
        exit(8);
    }

    fclose(fp);
    */

    int fd = open(curr_temp_file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Cannot open temporary file\n");
        exit(6);
    }
    unlink(curr_temp_file);

    int length = (int) lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    char* buffer = calloc(length, sizeof(char)); // sizeof(char) = 1, obviously
    if (!buffer) {
        fprintf(stderr, "Cannot allocate block\n");
        exit(7);
    }

    if (read(fd, buffer, length) != length) {
        fprintf(stderr, "Cannot read temporary file\n");
        exit(8);
    }

    close(fd);

    table[index] = buffer;
    return index;
}

void remove_block(int index) {
    if (index >= table_size) {
        fprintf(stderr, "Block index out of range\n");
        exit(9);
    }
    if (table[index] == NULL) {
        fprintf(stderr, "Block is empty\n");
        exit(10);
    }
    free(table[index]);
    table[index] = NULL;
}

void free_table(void) {
    if (table) {
        int i;
        for (i = 0; i < table_size; i++) {
            if (table[i]) {
                free(table[i]);
            }
        }
        free(table);
    }
    if (curr_dir) {
        free(curr_dir);
    }
    if (curr_file) {
        free(curr_file);
    }
    if (curr_temp_file) {
        free(curr_temp_file);
    }
}