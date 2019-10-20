#include "common.h"

mqd_t s_mqdes = -1; // server message queue descriptor

struct client {
    int id;
    mqd_t mqdes;
    int friend;
    struct client *next;
} *clients = NULL; // list of clients

int clients_id_autoincrement = 1; // id for next client

// find client by id
struct client *get_client(int c_id) {
    struct client *client = clients;
    while (client) {
        if (client->id == c_id) {
            return client;
        }
        client = client->next;
    }
    return NULL;
}

void init(char *c_mqname) {
    // open client msg queue key
    mqd_t c_mqdes;
    if ((c_mqdes = mq_open(c_mqname, O_WRONLY)) == -1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // add client to list
    struct client *client = malloc(sizeof(struct client));
    client->id = clients_id_autoincrement++;
    client->mqdes = c_mqdes;
    client->friend = 0;

    // append to list
    client->next = NULL;
    if (clients == NULL) {
        clients = client;
    } else {
        struct client *head = clients;
        while (head->next) {
            head = head->next;
        }
        head->next = client; 
    }

    // send client id to client
    char msg[sizeof(client->id)];
    memcpy(msg, &client->id, sizeof(client->id));
    if (mq_send(client->mqdes, msg, 100, MSG_INIT) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }
}

void echo(int c_id, char *string) {
    // find client
    struct client *client = get_client(c_id);
    if (client == NULL) {
        fprintf(stderr, "Client ID %d not found\n", c_id);
        exit(EXIT_FAILURE);
    }

    // send ECHO msg to client
    char msg[MSG_MAX_LENGTH];
    strcpy(msg, string);
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    strftime(msg+strlen(msg), 21, "\n%F %T", timeinfo); // 1+strlen(%F)+1+strlen(%T)+1=21
    if (mq_send(client->mqdes, msg, strlen(msg)+1, MSG_ECHO) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }
}

void list(int c_id) {
    // find client
    struct client *client = get_client(c_id);
    if (client == NULL) {
        fprintf(stderr, "Client ID %d not found\n", c_id);
        exit(EXIT_FAILURE);
    }

    // send LIST msg to client
    char msg[MSG_MAX_LENGTH];
    if (clients == NULL) {
        strcpy(msg, "No active clients");
    } else {
        snprintf(msg, sizeof(msg), "%s\t%s\t%s", "id", "mqdes", "friend");
        int counter = 0;
        struct client *client = clients;
        while (client) {
            char buf[MSG_MAX_LENGTH];
            snprintf(buf, sizeof(buf), "\n%d\t%d\t%s", client->id, client->mqdes, client->friend ? "Yes" : "No");
            strcat(msg, buf);
            counter++;
            client = client->next;
        }
    }
    if (mq_send(client->mqdes, msg, strlen(msg)+1, MSG_LIST) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }
}

void modify(char *list, int friend) {
    char *tok;
    tok = strtok(list, " ");
    while (tok != NULL) {
        int c_id = strtol(tok, NULL, 10);        
        struct client *client = get_client(c_id);
        if (client == NULL) {
            fprintf(stderr, "Client ID %d not found\n", c_id);  
        } else {
            client->friend = friend;
        }

        tok = strtok(NULL, " ");
    }
}

inline void add(char *list) {
    modify(list, 1);
}

inline void del(char *list) {
    modify(list, 0);
}

void del_all() {
    // delete all friends
    struct client *client = clients;
    while (client) {
        client->friend = 0;
        client = client->next;
    }
}

inline void friends(char *list) {
    del_all();
    add(list);
}

int prepare_msg(char *msg, int from_c_id, char *string) {
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    strcpy(msg, string);
    snprintf(msg+strlen(msg), MSG_MAX_LENGTH, "\nfrom client ID %d\n", from_c_id);
    strftime(msg+strlen(msg), 20, "%F %T", timeinfo); // strlen(%F)+1+strlen(%T)+1=20
    return 0;
}

void send_2one(int from_c_id, char *args) {
    // get client id
    char *tok = strtok(args, " ");
    if (tok == NULL) {
        fprintf(stderr, "Invalid client ID\n");
        return;
    }
    int c_id = strtol(tok, NULL, 10);        
    struct client *client = get_client(c_id);
    if (client == NULL) {
        fprintf(stderr, "Client ID %d not found\n", c_id);
        return;
    }

    // get message string
    char *string = args+strlen(tok)+1;
    if (string == NULL) {
        string = "";
    }

    // prepare msg to send
    char msg[MSG_MAX_LENGTH];
    prepare_msg(msg, from_c_id, string);

    // send msg to one client
    if (mq_send(client->mqdes, msg, strlen(msg)+1, MSG_2ONE) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

void send_2many(int from_c_id, char *string, int must_be_a_friend) {
    // prepare msg to send
    char msg[MSG_MAX_LENGTH];
    prepare_msg(msg, from_c_id, string);

    // send msg to all clients
    struct client *client = clients;
    while (client) {
        if (!must_be_a_friend || client->friend) {            
            if (mq_send(client->mqdes, msg, strlen(msg)+1, must_be_a_friend ? MSG_2FRIENDS : MSG_2ALL) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
        }
        client = client->next;
    }
}

inline void send_2all(int from_c_id, char *string) {
    send_2many(from_c_id, string, 0);
}

inline void send_2friends(int from_c_id, char *string) {
    send_2many(from_c_id, string, 1);
}

void stop(int c_id) {
    if (clients == NULL) {
        return;
    }
    if (clients->id == c_id) {
        struct client *del = clients;
        clients = clients->next;
        free(del);
        return;
    }
    struct client *client = clients;
    while (client->next) {
        if (client->next->id == c_id) {
            client->next = client->next->next;
            free(client->next);
            return;
        }
        client = client->next;
    }
    fprintf(stderr, "Client ID %d not found\n", c_id);
}

void cleanup() {
    if (s_mqdes != -1) {
        // stop clients and delete clients list
        struct client *client = clients;
        while (client) {
            struct client *next = client->next;
            
            // send STOP msg to client
            if (mq_send(client->mqdes, "", 1, MSG_STOP) == -1) {
                perror("mq_send");
            }

            // receive STOP msg from client
            printf("Waiting for client ID %d to send STOP...\n", client->id);
            char msg[MSG_MAX_LENGTH];
            unsigned prio;
            if (mq_receive(s_mqdes, msg, sizeof(msg), &prio) == -1) {
                perror("mq_receive");
            }
            if (prio != MSG_STOP) {
                fprintf(stderr, "Invalid message type\n");
            }

            // delete client
            free(client);            
            client = next;
        }
        clients = NULL;
        
        // close msg queue
        if (mq_close(s_mqdes) == -1) {
            perror("mq_close");
            _exit(EXIT_FAILURE);
        }

        // delete msg queue
        if (mq_unlink(SERVER_MSG_QUEUE_NAME) == -1) {
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

    
    // check if msg queue exists
    mqd_t mqdes_tmp;
    if ((mqdes_tmp = mq_open(SERVER_MSG_QUEUE_NAME, O_RDONLY)) != -1) {
        if (mq_close(mqdes_tmp) == -1) {
            perror("mq_close");
        }
        printf("\nServer is already running or message queue already exists. \
            \nPlease execute the following command: \
            \n> rm /dev/mqueue%s \
            \nThank you for your cooperation!\n\n", SERVER_MSG_QUEUE_NAME);
        exit(EXIT_FAILURE);
    }
    
    // display msg queue name
    printf("s_mqname=%s\n\n", SERVER_MSG_QUEUE_NAME);

    // create msg queue
    if ((s_mqdes = mq_open(SERVER_MSG_QUEUE_NAME, O_RDONLY | O_CREAT | O_EXCL, 0666, NULL)) == -1) {
        if (errno == EEXIST) {
            errno = 0;
            if ((s_mqdes = mq_open(SERVER_MSG_QUEUE_NAME, O_RDONLY)) == -1) {
                perror("mq_open");
                return EXIT_FAILURE;
            }
        } else {
            perror("mq_open");
            return EXIT_FAILURE;
        }        
    }

    // receive messages from clients
    while (1) {
        unsigned prio;
        char msg[MSG_MAX_LENGTH];
        switch (mq_receive(s_mqdes, msg, sizeof(msg), &prio)) {
            case -1:
                perror("mq_receive");
                break;

            default:
                switch (prio) {
                    case MSG_INIT:
                    {
                        char *c_mqname = strdup(msg);
                        printf("INIT from c_mqname=%s\n", c_mqname);
                        init(c_mqname);
                        free(c_mqname);
                        break;
                    }

                    default:
                    {         
                        char *tok = strtok(msg, " ");
                        if (tok == NULL) {
                            fprintf(stderr, "Invalid message format\n");
                            break;
                        }
                        int c_id = strtol(tok, NULL, 10);        

                        char *args = msg+strlen(tok)+1;
                        if (args == NULL) {
                            args = "";
                        }

                        switch (prio) {
                            case MSG_ECHO:
                                printf("ECHO from cid=%d \"%s\"\n", c_id, args);
                                echo(c_id, args);
                                break;

                            case MSG_LIST:
                                printf("LIST from cid=%d\n", c_id);
                                list(c_id);
                                break;

                            case MSG_FRIENDS:
                                printf("FRIENDS from cid=%d \"%s\"\n", c_id, args);
                                friends(args);
                                list(c_id);
                                break;

                            case MSG_ADD:
                                printf("ADD from cid=%d \"%s\"\n", c_id, args);
                                add(args);
                                list(c_id);
                                break;

                            case MSG_DEL:
                                printf("DEL from cid=%d \"%s\"\n", c_id, args);
                                del(args);
                                list(c_id);
                                break;

                            case MSG_2ALL:
                                printf("2ALL from cid=%d \"%s\"\n", c_id, args);
                                send_2all(c_id, args);
                                break;

                            case MSG_2FRIENDS:
                                printf("2FRIENDS from cid=%d \"%s\"\n", c_id, args);
                                send_2friends(c_id, args);
                                break;
                                
                            case MSG_2ONE:
                                printf("2ONE from cid=%d \"%s\"\n", c_id, args);
                                send_2one(c_id, args);
                                break;

                            case MSG_STOP:
                                printf("STOP from cid=%d\n", c_id);
                                stop(c_id);
                                break;

                            default:
                                fprintf(stderr, "Unsupported message type %d\n", prio);
                        }
                    }
                }
        }
    }

    return EXIT_SUCCESS;
}
