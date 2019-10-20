#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define PERROR(EXPR) if (EXPR == -1) { perror(NULL); _exit(EXIT_FAILURE); } // display errno message and exit if something goes wrong

// #define CONCURRENT // uncomment this line to execute each line concurrently

static inline void rtrim(char *str) { // trim to first endline (CR, LF or CRLF)
    while (*str && ((!(*str == '\n' || *str == '\r') && str++) || (*str = '\0'))); // lazy evaluation
}

static inline int stroccur(char *str, const char c) { // count number of character occurrences in string
    int n = 0;
    while (*str && (*str++ != c || ++n)); // lazy evaluation
    return n;
}

static char **strsplit(char *str, const char c) { // split string by character (modifies given string)
    char **tokens = malloc((stroccur(str, c) + 2) * sizeof(char*)); // n delimiters => n+1 tokens => n+2 elements (including NULL sentinel)
    char *token = strtok(str, &c);
    int i = 0;
    while (token != NULL) {
        tokens[i++] = token;
        token = strtok(NULL, &c);
    }
    tokens[i] = NULL; // exec arglist sentinel
    return tokens;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *path = realpath(argv[1], NULL);
    if (path == NULL) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(path, "r");
    free(path);
    if (fp == NULL) {
        perror("Cannot open script file");
        exit(EXIT_FAILURE);
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        rtrim(line); // trim to endline
        
        pid_t child_pid;
        switch (child_pid = fork()) {
            case -1:
                perror("Fork failed");
                exit(EXIT_FAILURE);

            case 0: // child process for line
                { // the curly braces are needed to avoid a compiler error
                    int prev_fd[2], next_fd[2];
                    char** const cmds = strsplit(line, '|'), **cmdptr = cmds;
                    while (*cmdptr) {
                        if (*(cmdptr+1)) { // if there is a next command
                            PERROR(pipe(next_fd));
                        }

                        char** const args = strsplit(*cmdptr, ' '); // (char*) cmd = *cmdptr;
                        switch (vfork()) { // or: fork()?
                            case -1:
                                perror("Fork failed");
                                _exit(EXIT_FAILURE);
                                
                            case 0: // child process for command
                                if (cmdptr != cmds) { // if there is a previous command
                                    PERROR(close(prev_fd[1]));
                                    PERROR(dup2(prev_fd[0], 0)); // STDIN_FILENO
                                    PERROR(close(prev_fd[0]));
                                }
                                if (*(cmdptr+1)) { // if there is a next command
                                    PERROR(close(next_fd[0]));
                                    PERROR(dup2(next_fd[1], 1)); // STDOUT_FILENO
                                    PERROR(close(next_fd[1]));
                                }
                                execvp(args[0], args); // prog = args[0] = *args
                                perror("Exec failed");
                                _exit(EXIT_FAILURE);

                            default: // parent process
                                if (cmdptr != cmds) { // if there is a previous command
                                    PERROR(close(prev_fd[0]));
                                    PERROR(close(prev_fd[1]));
                                }                                
                                // if (*(cmdptr+1)) { // if there is a next command
                                    prev_fd[0] = next_fd[0];
                                    prev_fd[1] = next_fd[1];
                                // }
                        }
                        free(args);
                        cmdptr++; // go to next command
                    }
                    free(cmds);
                    while (wait(NULL) != -1); // wait for all commands in this line to execute
                }
                _exit(EXIT_SUCCESS);

            #ifndef CONCURRENT
                default: // parent process
                    waitpid(child_pid, NULL, 0); // wait for this line to execute
            #endif
        }
    }

    if (fclose(fp) != 0) {
        perror("Cannot close script file");
        exit(EXIT_FAILURE);
    }

    #ifdef CONCURRENT
        while (wait(NULL) != -1); // wait for all lines to execute
    #endif

    exit(EXIT_SUCCESS);
}
