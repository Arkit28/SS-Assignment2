
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include "udp.h"
#include "chat_structures.h"
#include "list_helpers.c"


//server commands
void* listener_thread(void* arg);
void* worker_thread(void* arg);

void print_tokens(char *args[], int argc); // debug function
void parse_command(char request[], char command[], char arguments[]);
int classify_command(char *args, int argc);

//main commands
void handle_conn(ServerContext *server, struct sockaddr_in *client_addr, char *name);
void handle_disconn(ServerContext *server, struct sockaddr_in *client_addr);
void handle_message(ServerContext *server, struct sockaddr_in *client_addr, char *message);
void handle_rename(ServerContext *server, struct sockaddr_in *client_addr, char *new_name);
void handle_mute(ServerContext *server, struct sockaddr_in *client_addr, char *muted_name);
void handle_unmute(ServerContext *server, struct sockaddr_in *client_addr, char *unmuted_name);
void handle_private_message(ServerContext *server, struct sockaddr_in *client_addr, char *message, char *recipient_name);
void handle_kick_request(ServerContext *server, struct sockaddr_in *client_addr, char *target_name);

//comannd helpers
void send_all(ServerContext *server, char *msg); 
void send_error(ServerContext *server, struct sockaddr_in *client_addr, char *error_msg); 
void send_specific(ServerContext *server, struct sockaddr_in *client_addr, char *msg);
void admin_kick_confirmation(char *target_name);

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

    //initialise locks
    pthread_rwlock_init(&server.client_list_lock, NULL);
    pthread_rwlock_init(&server.mute_list_lock, NULL);


    printf("Server Started!\n");

    //spawn listener thread 
    pthread_create(&listenerThread, NULL, listener_thread, &server);

    pthread_join(listenerThread, NULL);

    // Server main loop
    
    // destroy locks, free lists and close socket before exiting
    pthread_rwlock_destroy(&server.client_list_lock);
    pthread_rwlock_destroy(&server.mute_list_lock);
    list_free_all(&server.ClientListHead);
    mute_free_all(&server.MutedListHead);
    close(server.socket_fd);

    return 0;
}


void parse_command(char request[], char command[], char arguments[]) {
    char *space = strchr(request, ' ');

    if (!space) {
        strcpy(command, request);
        arguments[0] = '\0';
        return;
    }

    // Split into two parts
    *space = '\0';      
    strcpy(command, request);

    strcpy(arguments, space + 1);
}

int classify_command(char *args, int argc){
    //args[0]= tolower(args[0]);
    if(strncmp("conn$", args, 5) == 0){
        return CONNECT;
    }
    else if(strncmp("disconn$", args, 8) == 0){
        return DISCONNECT;
    }
    else if(strncmp("say$", args, 4) == 0){
        return MESSAGE;
    }
    else if(strncmp("sayto$", args, 6) == 0){
        return PRIVATE_MESSAGE;
    }
    else if(strncmp("mute$", args, 5) == 0){
        return MUTE;
    }
    else if(strncmp("unmute$", args, 7) == 0){
        return UNMUTE;
    }
    else if(strncmp("rename$", args, 7) == 0){
        return RENAME;
    }
    else if(strncmp("kick$", args, 5) == 0){
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

    char command[BUFFER_SIZE];
    char arguments[BUFFER_SIZE];
    int command_type = -1;

    //printf("Worker thread has pulled up \n");

    char request_copy[BUFFER_SIZE];
    strncpy(request_copy, args->request, BUFFER_SIZE - 1);
    request_copy[BUFFER_SIZE - 1] = '\0';

    parse_command(args->request, command, arguments);
    //print_tokens(comman, argc);
    command_type = classify_command(command, 0);
    //printf("command type:%d\n", command_type);

    
    //check client is connected for commands except CONNECT
    if(command_type != CONNECT){
        pthread_rwlock_rdlock(&args->server->client_list_lock);
        ClientNode* client = list_find_by_address(args->server->ClientListHead, &args->client_addr);
        pthread_rwlock_unlock(&args->server->client_list_lock);
        if(!client){
            send_error(args->server, &args->client_addr, "Error: You must CONNECT before sending other commands.\n");
            free(args);
            return NULL;
        }
    }
    

    switch (command_type) {
        case CONNECT:
            printf("Client: %d wants to connect\n", ntohs(args->client_addr.sin_port));
            if(arguments[0] != '\0'){
                handle_conn(args->server, &args->client_addr, arguments);
            }
            else{
                send_error(args->server, &args->client_addr, "CONNECT command requires a name");
            }
            
            break;

        case DISCONNECT:
            printf("Client: %d wants to disconnect\n", ntohs(args->client_addr.sin_port));
            
            if(arguments != NULL){
                handle_disconn(args->server, &args->client_addr);
            }
            
            else{
            send_error(args->server, &args->client_addr, "DISCONNECT command requires no additional arguments");   //TODO
            }
            
            break;

        case MESSAGE:
            printf("Client: %d wants to send a message\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                handle_message(args->server, &args->client_addr, arguments);
            }
            
            else{
            send_error(args->server, &args->client_addr, "SAY command requires message. ");
            }
            
            break;
        case PRIVATE_MESSAGE:
            printf("Client: %d wants to send a private message\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                //parse recipient and message
                char *space = strchr(arguments, ' ');
                if(space){
                    *space = '\0';
                    char *recipient_name = arguments;
                    char *private_message = space + 1;
                    handle_private_message(args->server, &args->client_addr, private_message, recipient_name);
                }
                else{
                    send_error(args->server, &args->client_addr, "SAYTO command requires recipient name and message");
                }
            }
            
            else{
                send_error(args->server, &args->client_addr, "SAYTO command requires recipient name and message");
            }

            break;
        case MUTE:
            printf("Client: %d wants to mute someone\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                handle_mute(args->server, &args->client_addr, arguments);
            }
            
            else{
            send_error(args->server, &args->client_addr, "MUTE command requires a name");   //TODO
            }

            break;
        case UNMUTE:
            printf("Client: %d wants to unmute someone\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                handle_unmute(args->server, &args->client_addr, arguments);
            }
            
            else{
                send_error(args->server, &args->client_addr, "UNMUTE command requires a name");   //TODO
            }

            break;
        case RENAME:
            printf("Client: %d wants to rename themself\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                handle_rename(args->server, &args->client_addr, arguments);
            }
            
            else{
                send_error(args->server, &args->client_addr, "RENAME command requires a new name");   //TODO
            }
            

            break;
        case KICK_REQUEST:
            printf("Client: %d wants to kick someone\n", ntohs(args->client_addr.sin_port));

            if(arguments != NULL){
                handle_kick_request(args->server, &args->client_addr, arguments);
            }
            
            else{
                send_error(args->server, &args->client_addr, "KICK command requires a target name");   //TODO
            }

            break;
        default:
            printf("Client: %d sent an unknown command\n", ntohs(args->client_addr.sin_port));
            break;
        }
      
    //char server_response[BUFFER_SIZE];
    //strcpy(server_response, "Hi, the server has received: ");
    //strncat(server_response, request_copy, BUFFER_SIZE - strlen(server_response) - 1);
    //strncat(server_response, "\n", BUFFER_SIZE - strlen(server_response) - 1);
    //int rc = udp_socket_write(args->server->socket_fd, &args->client_addr, server_response, BUFFER_SIZE);
    

    free(args);
    //printf("Worker thread has done the work \n");
    return NULL;
}

void handle_conn(ServerContext *server, struct sockaddr_in *client_addr, char *name){
    // 0-lock client list for writing
    pthread_rwlock_wrlock(&server->client_list_lock);

    // 1-create new client node and add to client list
    // 2-set client addr and name
    list_add_client(&server->ClientListHead, name, client_addr);
    //list_print_all(server->ClientListHead);

    printf("Handled CONNECT for %s\n", name);
    
    // unlock client list BEFORE calling send_all to avoid deadlock
    pthread_rwlock_unlock(&server->client_list_lock);

    // 3-broadcast to all other clients that a new client has joined
    char msg[BUFFER_SIZE];
    strcpy(msg, name);
    strncat(msg, " has joined the chat!\n", BUFFER_SIZE - strlen(msg)-1);
    send_all(server, msg);
}

void handle_disconn(ServerContext *server, struct sockaddr_in *client_addr){
    // 0-lock client list for writing
    pthread_rwlock_wrlock(&server->client_list_lock);

    // 1-find client node by addr
    // 2-remove client node from client list
    // 3-send client acknowledgement message
    // 4-broadcast to all other clients that a client has left

    char msg[BUFFER_SIZE];
    char name_copy[MAX_NAME_LEN];

    ClientNode* disconnected_client = list_find_by_address(server->ClientListHead, client_addr);
    if(!disconnected_client){
        pthread_rwlock_unlock(&server->client_list_lock);
        send_error(server, client_addr, "Error: Disconnect failed.\n");
        return;
    }
    
    // copy name before removing node
    strncpy(name_copy, disconnected_client->name, MAX_NAME_LEN - 1);
    name_copy[MAX_NAME_LEN - 1] = '\0';
    if (name_copy[0] == '\0') {
        snprintf(name_copy, sizeof(name_copy), "client-%d", ntohs(client_addr->sin_port));
    }

    list_remove_client(&server->ClientListHead, client_addr);
    //list_print_all(server->ClientListHead);

    printf("Handled DISCONNECT\n");

    //unlock client list
    pthread_rwlock_unlock(&server->client_list_lock);

    // tell the disconnecting client
    char bye_msg[] = "Disconnected. Bye!";
    udp_socket_write(server->socket_fd, client_addr, bye_msg, strlen(bye_msg) + 1);

    // broadcast to everyone else
    snprintf(msg, sizeof(msg), "%s has left the chat!", name_copy);
    send_all(server, msg);
}

void handle_message(ServerContext *server, struct sockaddr_in *client_addr, char *message){
    // 0-lock client list for reading
    pthread_rwlock_rdlock(&server->client_list_lock);

    // 1-find client node by addr
    ClientNode* sender = list_find_by_address(server->ClientListHead, client_addr);
    
    if(!sender){
        pthread_rwlock_unlock(&server->client_list_lock);
        send_error(server, client_addr, "Error: You must connect first.\n");
        return;
    }

    printf("%s: %s\n", sender->name, message);
    
    // build formatted message with sender's name
    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", sender->name, message);
    
    //unlock client list before calling send_specific to avoid deadlock
    pthread_rwlock_unlock(&server->client_list_lock);

    // 2-broadcast message to all other clients 
    send_specific(server, client_addr, formatted_msg);
}

void handle_rename(ServerContext *server, struct sockaddr_in *client_addr, char *new_name){
    // 0-lock client list for writing
    pthread_rwlock_wrlock(&server->client_list_lock);

    // 1-find client node by addr
    // 2-update client name
    // 3-send client acknowledgement message
    // 4-broadcast to all other clients that a client has renamed themself

    if(list_find_by_address(server->ClientListHead, client_addr) != NULL){
        ClientNode* client = list_find_by_address(server->ClientListHead, client_addr);
        strncpy(client->name, new_name, MAX_NAME_LEN - 1);
        client->name[MAX_NAME_LEN - 1] = '\0';
    }
    list_print_all(server->ClientListHead);

    printf("Handled RENAME to %s\n", new_name);

    //unlock client list
    pthread_rwlock_unlock(&server->client_list_lock);
}

void handle_mute(ServerContext *server, struct sockaddr_in *client_addr, char *muted_name){
    // 0- lock muted list for write
    pthread_rwlock_wrlock(&server->mute_list_lock);

    // 1-verify both muter and muted exist in client list
    ClientNode* muter_node = list_find_by_address(server->ClientListHead, client_addr);
    ClientNode* muted_node = list_find_by_name(server->ClientListHead, muted_name);

    // 2 - add muted pair to muted list
    if(muter_node && muted_node){
        mute_add(&server->MutedListHead, muter_node->name, muted_node->name);
        printf("Handled MUTE: %s muted %s\n", muter_node->name, muted_node->name);
    }
    else{
        printf("MUTE failed: muter or muted not found\n");
    }

    // 3 - send acknowledgement to muter 
    char server_response[BUFFER_SIZE];
    strcpy(server_response, "You have muted ");
    strncat(server_response, muted_name, BUFFER_SIZE - strlen(server_response) - 1);
    strncat(server_response, "\n", BUFFER_SIZE - strlen(server_response) - 1);

    int rc = udp_socket_write(server->socket_fd, client_addr, server_response, BUFFER_SIZE);
    
    // 4 - unlock muted list
    pthread_rwlock_unlock(&server->mute_list_lock);

}

void handle_unmute(ServerContext *server, struct sockaddr_in *client_addr, char* unmuted_name){
    // 0- lock muted list for write
    pthread_rwlock_wrlock(&server->mute_list_lock);

    // 1-verify both muter and muted exist in client list
    ClientNode* muter_node = list_find_by_address(server->ClientListHead, client_addr);
    ClientNode* muted_node = list_find_by_name(server->ClientListHead, unmuted_name);

    // 2 - remove muted pair from muted list
    if(muter_node && muted_node){
        int result = mute_remove(&server->MutedListHead, muter_node->name, muted_node->name);
        if(result){
            printf("Handled UNMUTE: %s unmuted %s\n", muter_node->name, muted_node->name);
        }
        else{
            printf("UNMUTE failed: mute pairing not found\n");
        }
    }
    else{
        printf("UNMUTE failed: muter or muted not found\n");
    }

    // 3 - send acknowledgement to muter (TODO)
    char server_response[BUFFER_SIZE];
    strcpy(server_response, "You have unmuted ");
    strncat(server_response, unmuted_name, BUFFER_SIZE - strlen(server_response) - 1);
    strncat(server_response, "\n", BUFFER_SIZE - strlen(server_response) - 1);

    int rc = udp_socket_write(server->socket_fd, client_addr, server_response, BUFFER_SIZE);
    
    // 4 - unlock muted list
    pthread_rwlock_unlock(&server->mute_list_lock);
}

void handle_private_message(ServerContext *server, struct sockaddr_in *client_addr, char *message, char *recipient_name){
    // lock muted and client list for reading
    pthread_rwlock_rdlock(&server->mute_list_lock);
    pthread_rwlock_rdlock(&server->client_list_lock);

    // find sender and recipient nodes
    ClientNode* sender = list_find_by_address(server->ClientListHead, client_addr);
    ClientNode* recipient = list_find_by_name(server->ClientListHead, recipient_name);

    // form message and send to recipient if not muted
    char private_msg[BUFFER_SIZE];
    strcpy(private_msg, "[");
    strncat(private_msg, sender->name, BUFFER_SIZE - strlen(private_msg) - 1);
    strncat(private_msg, "]: ", BUFFER_SIZE - strlen(private_msg) - 1);
    strncat(private_msg, message, BUFFER_SIZE - strlen(private_msg) - 1);
    strncat(private_msg, "\n", BUFFER_SIZE - strlen(private_msg) - 1);

    if(sender && recipient){
        // check if theres a muted pairing
        MutedPair* current = server->MutedListHead;
        if(is_muted(server->MutedListHead, recipient->name, sender->name) ||
           is_muted(server->MutedListHead, sender->name, recipient->name)){
            printf("Private message from %s to %s blocked (muted)\n", sender->name, recipient->name);
            int rc = udp_socket_write(server->socket_fd, client_addr, "Your private message could not be delivered (muted)\n", BUFFER_SIZE);
        }
        else{
            int rc = udp_socket_write(server->socket_fd, &recipient->address, private_msg, BUFFER_SIZE);
            printf("Private message from %s to %s sent\n", sender->name, recipient->name);
        }
    }

    pthread_rwlock_unlock(&server->client_list_lock);
    pthread_rwlock_unlock(&server->mute_list_lock);
}

void send_all(ServerContext *server, char *msg){
    pthread_rwlock_rdlock(&server->client_list_lock);

    int len = (int)(strlen(msg) + 1);
    //loop through client list and send msg to each client
    ClientNode* current = server->ClientListHead;
    while(current){
        int rc = udp_socket_write(server->socket_fd, &current->address, msg, len);
        current = current->next;
    }

    pthread_rwlock_unlock(&server->client_list_lock);
}

void send_error(ServerContext *server, struct sockaddr_in *client_addr, char *error_msg){
    char error_buffer[BUFFER_SIZE];
    snprintf(error_buffer, sizeof(error_buffer), "ERROR: %s", error_msg);
    udp_socket_write(server->socket_fd, client_addr, error_buffer, BUFFER_SIZE);
}

void send_specific(ServerContext *server, struct sockaddr_in *client_addr, char *msg){
    pthread_rwlock_rdlock(&server->mute_list_lock);
    pthread_rwlock_rdlock(&server->client_list_lock);
    
    int len = (int)(strlen(msg) + 1);
    // find the sender to get their name
    ClientNode* sender = list_find_by_address(server->ClientListHead, client_addr);
    if(!sender) {
        pthread_rwlock_unlock(&server->client_list_lock);
        pthread_rwlock_unlock(&server->mute_list_lock);
        return;
    }

    // broadcast to all clients except those who muted the sender
    ClientNode* current_client = server->ClientListHead;
    while(current_client){
        
        // check if this recipient has muted the sender
        if(!is_muted(server->MutedListHead, current_client->name, sender->name)){
            udp_socket_write(server->socket_fd, &current_client->address, msg, len);
        }
        
        current_client = current_client->next;
    }

    pthread_rwlock_unlock(&server->client_list_lock);
    pthread_rwlock_unlock(&server->mute_list_lock);
}


void handle_kick_request(ServerContext *server, struct sockaddr_in *client_addr, char *target_name){
    // Lock client list for writing
    pthread_rwlock_wrlock(&server->client_list_lock);

    // Verify the target exists 
    ClientNode* target_client = list_find_by_name(server->ClientListHead, target_name);
    if (!target_client) {
        pthread_rwlock_unlock(&server->client_list_lock);
        send_error(server, client_addr, "KICK request failed: target client not found");
        return;
    }

    
    char kicked_name[MAX_NAME_LEN];
    strncpy(kicked_name, target_name, MAX_NAME_LEN - 1);
    kicked_name[MAX_NAME_LEN - 1] = '\0';

    //is an admin connected?
    ClientNode* admin = list_find_by_name(server->ClientListHead, "admin");
    
    if (admin != NULL && ntohs(admin->address.sin_port) == 6666) {
        
        if (admin->address.sin_port == client_addr->sin_port) {
            //skip confirmation if admin is kicking some1
            printf("Admin requested to kick %s. Processing immediately.\n", kicked_name);
            list_remove_client(&server->ClientListHead, &target_client->address);
            pthread_rwlock_unlock(&server->client_list_lock);

            char kick_msg[BUFFER_SIZE];
            snprintf(kick_msg, sizeof(kick_msg), "%s has been kicked from the server.", kicked_name);
            send_all(server, kick_msg);
            printf("Client %s has been kicked.\n", kicked_name);
        } else {
            
            printf("Non-admin requested kick for %s. Asking admin.\n", kicked_name);
            pthread_rwlock_unlock(&server->client_list_lock);

            char kick_msg[BUFFER_SIZE];
            sprintf(kick_msg, "KICK REQUEST: %s (perform 'kick$ %s' to confirm, or ignore to deny)",
                     kicked_name, kicked_name);
            udp_socket_write(server->socket_fd, &admin->address, kick_msg, BUFFER_SIZE);
        }
    } else {
        
        pthread_rwlock_unlock(&server->client_list_lock);
        send_error(server, client_addr, "No admin connected to approve kick request");
    }
}


