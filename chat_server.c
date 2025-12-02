
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include "udp.h"
#include "chat_structures.h"
#include "list_helpers.h"


//server commands
void* listener_thread(void* arg);
void* worker_thread(void* arg);

void print_tokens(char *args[], int argc); // debug function
void parse_command(char request[], char *args[], int *argc);
int classify_command(char *args[], int argc);

void handle_conn(ServerContext *server, struct sockaddr_in *client_addr, char *name);
void handle_disconn(ServerContext *server, struct sockaddr_in *client_addr);

int main(int argc, char *argv[])
{
    ServerContext server;


    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number SERVER_PORT
    // (See details of the function in udp.h)
    server.socket_fd = udp_socket_open(SERVER_PORT);

    assert(server.socket_fd > -1);

    // initialise client connection list and muted list
    server.ClientListHead = NULL;
    server.MutedListHead = NULL;

    //initialise threads
    pthread_t listenerThread;
    pthread_t workerThread;

    //initialise locks
    pthread_rwlock_init(&server.client_list_lock, NULL);
    pthread_rwlock_init(&server.mute_list_lock, NULL);


    printf("Server Started!\n");

    //spawn listener thread 
    pthread_create(&listenerThread, NULL, listener_thread, &server);

    pthread_join(listenerThread, NULL);
    pthread_join(workerThread, NULL);

    // Server main loop
    

    return 0;
}


void parse_command(char request[], char* args[], int* argc){
    // parse command from client request
    // arg[0] = command
    
    char* token = strtok(request, " ");
    *argc = 0;
    while(token != NULL){
        args[(*argc)++] = token;
        token = strtok(NULL, " ");
    }

    args[*argc] = NULL;
}

void print_tokens(char *args[], int argc){
    printf("parsed %d tokens:\n", argc);
    for(int i = 0; i < argc; ++i){
        printf("    Token[%d]: %s\n", i, args[i]);
    }
}

int classify_command(char *args[], int argc){
    //args[0]= tolower(args[0]);
    if(strncmp("conn$", args[0], 5) == 0){
        return CONNECT;
    }
    else if(strncmp("disconn$", args[0], 8) == 0){
        return DISCONNECT;
    }
    else if(strncmp("say$", args[0], 4) == 0){
        return MESSAGE;
    }
    else if(strncmp("sayto$", args[0], 6) == 0){
        return PRIVATE_MESSAGE;
    }
    else if(strncmp("mute$", args[0], 5) == 0){
        return MUTE;
    }
    else if(strncmp("unmute$", args[0], 7) == 0){
        return UNMUTE;
    }
    else if(strncmp("rename$", args[0], 7) == 0){
        return RENAME;
    }
    else if(strncmp("kick$", args[0], 5) == 0){
        return KICK_REQUEST;            //NOTE : admin verfication is needed
    }

    return -1;  //unknown command
}


void* listener_thread(void* arg){

    ServerContext* server = (ServerContext*) arg;

    printf("Listener thread has pulled up \n");


    while (1) 
    {
        // Storage for request and response messages
        char client_request[BUFFER_SIZE] ;

        // Variable to store incoming client's IP address and port
        struct sockaddr_in client_address;
    
        // This function reads incoming client request from
        // the socket at sd.
        int rc = udp_socket_read(server->socket_fd, &client_address, client_request, BUFFER_SIZE);

        /* only process when a packet was actually received */
        if (rc > 0) {
            /* ensure null-termination using the returned length */
            if(rc < BUFFER_SIZE){
                client_request[rc] = '\0';
            } 
            else{
                client_request[BUFFER_SIZE-1] = '\0';
            }

            // package arguments for worker thread
            WorkerArgs *args = malloc(sizeof(WorkerArgs));
            if(!args){
                perror("Failed to allocate memory for worker args");
                continue;
            }

            args->server = server;
            args->client_addr = client_address;
            strncpy(args->request, client_request, BUFFER_SIZE - 1);
            args->request[BUFFER_SIZE - 1] = '\0'; 

            //spawn worker thread:
            pthread_t worker_tid;
            if(pthread_create(&worker_tid, NULL, worker_thread, (void*)args) != 0){    
                perror("Failed to create worker thread");
                free(args);
                continue;
            }

            pthread_detach(worker_tid);
            

            

            /* Demo code (remove later) - build response using original message (preserves spaces) 
            strcpy(server_response, "Hi, the server has received: ");
            strncat(server_response, client_request, BUFFER_SIZE - strlen(server_response) - 1);
            strncat(server_response, "\n", BUFFER_SIZE - strlen(server_response) - 1);

            printf("%d: %s\n", ntohs(client_address.sin_port), client_request);

            rc = udp_socket_write(server->socket_fd, &client_address, server_response, BUFFER_SIZE);
            */
        }
    }

}

void* worker_thread(void* arg){
    WorkerArgs* args = (WorkerArgs*) arg;

    char* tokens[BUFFER_SIZE];
    int argc = 0;
    int command_type = -1;

    printf("Worker thread has pulled up \n");

    char request_copy[BUFFER_SIZE];
    strncpy(request_copy, args->request, BUFFER_SIZE - 1);
    request_copy[BUFFER_SIZE - 1] = '\0';

    parse_command(args->request, tokens, &argc);
    //print_tokens(args, argc);
    command_type = classify_command(tokens, argc);
    //printf("command type:%d\n", command_type);

    

    switch (command_type) {
        case CONNECT:
            printf("Client: %d wants to connect\n", ntohs(args->client_addr.sin_port));
            break;
        case DISCONNECT:
            printf("Client: %d wants to disconnect\n", ntohs(args->client_addr.sin_port));
            break;
        case MESSAGE:
            printf("Client: %d wants to send a message\n", ntohs(args->client_addr.sin_port));
            break;
        case PRIVATE_MESSAGE:
            printf("Client: %d wants to send a private message\n", ntohs(args->client_addr.sin_port));
            break;
        case MUTE:
            printf("Client: %d wants to mute someone\n", ntohs(args->client_addr.sin_port));
            break;
        case UNMUTE:
            printf("Client: %d wants to unmute someone\n", ntohs(args->client_addr.sin_port));
            break;
        case RENAME:
            printf("Client: %d wants to rename themself\n", ntohs(args->client_addr.sin_port));
            break;
        case KICK_REQUEST:
            printf("Client: %d wants to kick someone\n", ntohs(args->client_addr.sin_port));
            break;
        default:
            printf("Client: %d sent an unknown command\n", ntohs(args->client_addr.sin_port));
            break;
        }

        char server_response[BUFFER_SIZE];

        strcpy(server_response, "Hi, the server has received: ");
        strncat(server_response, request_copy, BUFFER_SIZE - strlen(server_response) - 1);
        strncat(server_response, "\n", BUFFER_SIZE - strlen(server_response) - 1);

        printf("%d: %s\n", ntohs(args->client_addr.sin_port), request_copy);

        int rc = udp_socket_write(args->server->socket_fd, &args->client_addr, server_response, BUFFER_SIZE);
    


    free(arg);
    printf("Worker thread has done the work \n");
    return NULL;
}