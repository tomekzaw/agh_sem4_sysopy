#include "common.h"

int s_msqid = -1; // server message queue id

struct client {
    int id;
    int msqid;
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

void init(int c_msqid) {   
    // add client to list
    struct client *client = malloc(sizeof(struct client));
    client->id = clients_id_autoincrement++;
    client->msqid = c_msqid;
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
    struct msgbuf msg;
    msg.mtype = client->msqid;
    memcpy(msg.mtext, &client->id, sizeof(client->id));
    if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
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
    struct msgbuf msg;
    msg.mtype = client->msqid;
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    strcpy(msg.mtext, string);
    strftime(msg.mtext+strlen(string), 21, "\n%F %T", timeinfo); // 1+strlen(%F)+1+strlen(%T)+1=21
    if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
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
    struct msgbuf msg;
    msg.mtype = client->msqid;
    if (clients == NULL) {
        strcpy(msg.mtext, "No active clients");
    } else {
        snprintf(msg.mtext, sizeof(msg.mtext), "%s\t%s\t\t%s", "id", "msqid", "friend");
        int counter = 0;
        struct client *client = clients;
        while (client) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "\n%d\t%d\t%s", client->id, client->msqid, client->friend ? "Yes" : "No");
            strcat(msg.mtext, buf);
            counter++;
            client = client->next;
        }
    }
    if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
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

int prepare_msg(struct msgbuf *msg, int from_c_id, char *string) {
    if (msg == NULL) {
        return -1;
    }
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    strcpy(msg->mtext, string);
    snprintf(msg->mtext+strlen(msg->mtext), sizeof(msg->mtext)-strlen(msg->mtext), "\nfrom client ID %d\n", from_c_id);
    strftime(msg->mtext+strlen(msg->mtext), 20, "%F %T", timeinfo); // strlen(%F)+1+strlen(%T)+1=20
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
    struct msgbuf msg;
    prepare_msg(&msg, from_c_id, string);

    // send msg to one client
    msg.mtype = client->msqid;
    if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
        perror("msgsnd");
    }
}

void send_2many(int from_c_id, char *string, int must_be_a_friend) {
    // prepare msg to send
    struct msgbuf msg;
    prepare_msg(&msg, from_c_id, string);

    // send msg to all clients
    struct client *client = clients;
    while (client) {
        if (!must_be_a_friend || client->friend) {            
            msg.mtype = client->msqid;
            if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
                perror("msgsnd");
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
    if (s_msqid != -1) {
        // stop clients and delete clients list
        struct client *client = clients;
        while (client) {
            struct client *next = client->next;
            
            // send STOP msg to client
            struct msgbuf msg;
            msg.mtype = MSG_STOP;
            if (msgsnd(client->msqid, &msg, sizeof(msg), 0) == -1) {
                perror("msgsnd");
            }

            // receive STOP msg from client
            printf("Waiting for client ID %d to send STOP...\n", client->id);
            if (msgrcv(s_msqid, &msg, sizeof(msg), MSG_STOP, 0) == -1) {
                perror("msgrcv");
            }

            // delete client
            free(client);            
            client = next;
        }
        clients = NULL;
        
        // remove server msg queue
        if (msgctl(s_msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            _exit(EXIT_FAILURE);
        }
    }

    printf("Goodbye!\n\n");
}

void sigint_handler() {
    printf("\nReceived SIGINT\n");
    cleanup();
    _exit(EXIT_SUCCESS);
}

void exit_handler() {
    // remove server msg queue
    if (s_msqid != -1) {
        if (msgctl(s_msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            _exit(EXIT_FAILURE);
        }
    }
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

    // generate server msg queue key
    key_t s_key;
    if ((s_key = SERVER_QUEUE_KEY()) == -1) {
        perror("ftok");
        return EXIT_FAILURE;        
    }
    printf("s_key=%d\n", s_key);

    // create server msg queue
    if ((s_msqid = msgget(s_key, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("msgget");
        printf("\nServer is already running or message queue already exists. \
            \nPlease execute the following command: \
            \n> ipcrm -Q %d \
            \nThank you for your cooperation!\n\n", s_key);
        return EXIT_FAILURE;
    }
    printf("s_msqid=%d\n", s_msqid);

    // receive messages from clients
    struct msgbuf msg;
    while (msgrcv(s_msqid, &msg, sizeof(msg), 0, 0) > 0) {
        switch (msg.mtype) {
            case MSG_INIT:
            {
                int c_msqid = *((int*) msg.mtext);
                printf("INIT from c_msqid=%d\n", c_msqid);
                init(c_msqid);
                break;
            }

            default:         
            {
                int c_id = *((int*) &msg.mtext);
                char *args = msg.mtext+sizeof(c_id);

                switch (msg.mtype) {
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
                        fprintf(stderr, "Unsupported message type %ld\n", msg.mtype);
                }
            }
        }        
    }

    return EXIT_SUCCESS; // avoid Wreturn-type
}
