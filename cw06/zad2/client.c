#include "common.h"

mqd_t s_mqdes = -1, c_mqdes = -1; // server and client message queue descriptor
char c_mqname[255] = ""; // client message queue name
int c_id = -1; // client id assigned by server
char cmd[MSG_MAX_LENGTH]; // buffer for commands entered by user

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
    char msg[MSG_MAX_LENGTH];
    snprintf(msg, sizeof(msg), "%d %s", c_id, args);
    if (mq_send(s_mqdes, msg, strlen(msg)+1, mtype) == -1) {
        perror("mq_send");
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
            printf("\n%c ", CLIENT_CMD_PROMPT_CHAR);
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
            || printf("Unknown command\n")); // avoid Wunused-value=
    }
}

void register_notification();

void receiver() {
    register_notification();

    unsigned prio;
    char msg[MSG_MAX_LENGTH];
    if (mq_receive(c_mqdes, msg, sizeof(msg), &prio) == -1) {
        perror("mq_receive");
        exit(EXIT_FAILURE);
    }

    switch (prio) {
        case MSG_STOP:
            printf("\rReceived STOP\n");
            exit(EXIT_SUCCESS);

        default:
            printf("\r \r%s\n\n%c ", msg, CLIENT_CMD_PROMPT_CHAR);
            fflush(stdout);
    }
}

void register_notification() {
    struct sigevent notification;
    notification.sigev_notify = SIGEV_THREAD;
    notification.sigev_notify_function = receiver;
    notification.sigev_notify_attributes = NULL;
    if (mq_notify(c_mqdes, &notification) == -1) {
        perror("mq_notify");
    }
}

void cleanup() {
    if (c_mqdes != -1) {
        // close client msg queue
        if (mq_close(c_mqdes) == -1) {
            perror("mq_close");
            _exit(EXIT_FAILURE);
        }
    }

    if (c_mqname[0] != '\0') {
        // delete client msg queue
        if (mq_unlink(c_mqname) == -1) {
            perror("mq_close");
            _exit(EXIT_FAILURE);
        }
    }

    if (s_mqdes != -1) {
        // send STOP msg to server
        char msg[MSG_MAX_LENGTH];
        snprintf(msg, sizeof(msg), "%d", c_id);
        if (mq_send(s_mqdes, msg, strlen(msg)+1, MSG_STOP) == -1) {
            perror("mq_send");
            exit(EXIT_FAILURE);
        }
        
        // close server msg queue
        if (mq_close(s_mqdes) == -1) {
            perror("mq_close");
            _exit(EXIT_FAILURE);
        }
    }

    printf("Goodbye!\n\n");
}

void exit_handler() {
    cleanup();
}

void sigint_handler() {    
    printf("\nReceived SIGINT\n");
    cleanup();
    _exit(EXIT_SUCCESS);
}

int main() {
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

    // open server msg queue key
    if ((s_mqdes = mq_open(SERVER_MSG_QUEUE_NAME, O_WRONLY)) == -1) {
        perror("mq_open");
        if (errno == ENOENT) {
            printf("\nPerhaps server is not running \
                \nPlease execute the following command: \
                \n> ./server\n\n");
        }
        return EXIT_FAILURE;
    }

    // generate msg queue name
    snprintf(c_mqname, sizeof(c_mqname), "%s_%d", CLIENT_MSG_QUEUE_NAME_PREFIX, getpid());
    printf("c_mqname=%s\n", c_mqname);

    // create client msg queue
    if ((c_mqdes = mq_open(c_mqname, O_RDONLY | O_CREAT | O_EXCL, 0666, NULL)) == -1) {
        perror("mq_open");
        return EXIT_FAILURE;
    }

    // send INIT msg to server
    if (mq_send(s_mqdes, c_mqname, strlen(c_mqname)+1, MSG_INIT) == -1) {
        perror("mq_send");
        return EXIT_FAILURE;
    }

    // receive client id from server
    char msg[MSG_MAX_LENGTH];
    if (mq_receive(c_mqdes, msg, sizeof(msg), NULL) == -1) {
        perror("mq_receive");
        return EXIT_FAILURE;
    }
    c_id = *((int*) msg);
    printf("c_id=%d\n", c_id);

    // register msg notification handler
    register_notification();

    // execute commands
    read_cmds(stdin);

    return EXIT_SUCCESS;
}
