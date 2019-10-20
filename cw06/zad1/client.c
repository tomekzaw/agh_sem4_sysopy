#include "common.h"

int s_msqid = -1, c_msqid = -1; // server and client message queue id
int c_id = -1; // client id assigned by server
char cmd[MSG_MAX_LENGTH]; // buffer for commands entered by user
char cmd_prompt = '>'; // prompt text for command line
pid_t child_pid = -1;

int handle_cmd(char *name, int mtype, void (*handler)(int, char*)) {
    unsigned length = strlen(name);

    // verify cmd length
    if (strlen(cmd) < length) {
        return 0;
    }

    // strtoupper on cmd
    char *cmd_uppercase = malloc(length);
    memcpy(cmd_uppercase, cmd, length);
    char *chr = cmd_uppercase+length;
    while (chr-- > cmd_uppercase) {
        *chr = toupper(*chr);
    }

    // compare with cmd name
    if (!memcmp(cmd_uppercase, name, length) == 0) {
        free(cmd_uppercase);
        return 0;
    }
    free(cmd_uppercase);

    // require space or end of string
    if (cmd[length] != ' ' && cmd[length] != '\0') {
        return 0;
    }

    // execute cmd
    char *args = cmd[length] == ' ' ? cmd+length+1 : "";
    handler(mtype, args);
    return 1;
}

void cmd_handler(int mtype, char *args) {
    // validate args
    switch (mtype) {
        case MSG_ADD:
        case MSG_DEL:
            if (strlen(args) == 0) {
                fprintf(stderr, "Empty friends modification list\n");
                return;
            }
    }

    // send msg to server
    struct msgbuf msg;
    msg.mtype = mtype;
    memcpy(msg.mtext, &c_id, sizeof(c_id));
    strcpy(msg.mtext+sizeof(c_id), args);
    if (msgsnd(s_msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

void stop() {
    exit(EXIT_SUCCESS); // -> cleanup()
}

void read_cmds(FILE*);   

void cmd_read(__attribute__((unused)) int mtype, char *path) {
    if (strlen(path) == 0) {
        fprintf(stderr, "Path not specified\n");
        return;
    }
    FILE* fp;
    if ((fp = fopen(path, "r")) == NULL) {
        perror("fopen");
        return;
    }
    read_cmds(fp);
    if (fclose(fp) != 0) {
        perror("fclose");
        return;
    }
}

void read_cmds(FILE *stream) {    
    while (1) {
        // display cmd prompt
        if (stream == stdin) {
            printf("\n%c ", cmd_prompt);
            fflush(stdout);
        }

        // read cmd
        if (fgets(cmd, sizeof(cmd), stream) == NULL) { // if EOF
            return;
        }

        // validate cmd
        int length = strlen(cmd);
        if (length == 1) {
            if (stream == stdin) {
				fprintf(stderr, "Command not specified\n");
			}
            continue;
        }
        cmd[length-1] = '\0'; // replace '\n' with '\0'

        // execute command
        (void) (handle_cmd("ECHO", MSG_ECHO, cmd_handler) 
            || handle_cmd("LIST", MSG_LIST, cmd_handler)
            || handle_cmd("FRIENDS", MSG_FRIENDS, cmd_handler)
            || handle_cmd("ADD", MSG_ADD, cmd_handler)
            || handle_cmd("DEL", MSG_DEL, cmd_handler)
            || handle_cmd("2ALL", MSG_2ALL, cmd_handler)
            || handle_cmd("2FRIENDS", MSG_2FRIENDS, cmd_handler)
            || handle_cmd("2ONE", MSG_2ONE, cmd_handler)
            || handle_cmd("STOP", 0, stop)
            || handle_cmd("READ", 0, cmd_read)
            || printf("Unknown command\n")); // avoid Wunused-value
    }
}

void cleanup() {
    // kill child
    if (child_pid != -1) {
        if (kill(child_pid, SIGINT) == -1) {
            perror("kill");
            _exit(EXIT_FAILURE);
        }
    }

    // remove client msg queue
    if (c_msqid != -1) {
        if (msgctl(c_msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            _exit(EXIT_FAILURE);
        }
    }

    // send STOP msg to server
    if (s_msqid != -1) {
        struct msgbuf msg;
        msg.mtype = MSG_STOP;
        memcpy(msg.mtext, &c_id, sizeof(c_id));
        if (msgsnd(s_msqid, &msg, sizeof(msg), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
    }

    printf("Goodbye!\n\n");
}

void exit_handler() {
    cleanup();
}

void signal_handler(int signo) {
    if (signo == SIGINT) {
        printf("\nReceived SIGINT\n");
    }
    cleanup();
    _exit(EXIT_SUCCESS);
}

int main() {
    // generate server msg queue key
    key_t s_key;
    if ((s_key = SERVER_QUEUE_KEY()) == -1) {
        perror("ftok");
        return EXIT_FAILURE;        
    }
    printf("s_key=%d\n", s_key);

    // open server msg queue key
    if ((s_msqid = msgget(s_key, 0)) == -1) {
        perror("msgget");
        if (errno == ENOENT) {
            printf("\nPerhaps server is not running \
                \nPlease execute the following command: \
                \n> ./server\n\n");
        }
        return EXIT_FAILURE;
    }
    printf("s_msqid=%d\n", s_msqid);

    // create client msg queue
    if ((c_msqid = msgget(IPC_PRIVATE, 0666)) == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }
    printf("c_msqid=%d\n", c_msqid);

    // send INIT msg to server
    struct msgbuf msg;
    msg.mtype = MSG_INIT;
    memcpy(msg.mtext, &c_msqid, sizeof(c_msqid));
    if (msgsnd(s_msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
        return EXIT_FAILURE;
    }

    // receive client id from server
    if (msgrcv(c_msqid, &msg, sizeof(msg), c_msqid, 0) == -1) {
        perror("msgrcv");
        return EXIT_FAILURE;
    }
    c_id = *((int*) &msg.mtext);
    printf("c_id=%d\n", c_id);
   
    // parent-client is responsible for executing commands
    // while child-client is responsible for receiving messages
    // alternative version is to receive message after a signal is received
    switch (child_pid = fork()) {
        case -1: // fork failed
            perror("fork");
            exit(EXIT_FAILURE);

        case 0: // child process
        {
            // receive messages from server        
            int receive = 1;
            while (receive) {
                struct msgbuf msg;
                switch (msgrcv(c_msqid, &msg, sizeof(msg), 0, 0)) {
                    case -1: // msgrcv failed
                        perror("msgrcv");
                        break;

                    default:
                        switch (msg.mtype) {
                            case MSG_STOP:
                                printf("\rReceived STOP\n");
                                if (kill(getppid(), SIGUSR1) == -1) {
                                    perror("kill");
                                    exit(EXIT_FAILURE);
                                }
                                exit(EXIT_SUCCESS);
                            
                            default:
                                printf("\r \r%s\n\n%c ", msg.mtext, cmd_prompt);
                                fflush(stdout);
                        }
                }
            }
        }

        default: // parent process
        {
            // register exit handler
            if (atexit(exit_handler) == -1) {
                perror("atexit");
                return EXIT_FAILURE;
            }

            // register SIGINT handler (Ctrl+C)
            struct sigaction act;
            act.sa_handler = signal_handler;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            if (sigaction(SIGINT, &act, NULL) == -1 || sigaction(SIGUSR1, &act, NULL) == -1) {
                perror("sigaction");
                return EXIT_FAILURE;
            }

            // execute commands
            read_cmds(stdin);
        }
    }

    return EXIT_SUCCESS; // avoid Wreturn-type
}
