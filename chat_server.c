
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


void* listener_thread(void* arg){

    ServerContext* server = (ServerContext*) arg;

    printf("Listener thread has pulled up \n");


    while (1) 
    {
        // Storage for request and response messages
        char client_request[BUFFER_SIZE], server_response[BUFFER_SIZE];

        // Variable to store incoming client's IP address and port
        struct sockaddr_in client_address;
    
        // This function reads incoming client request from
        // the socket at sd.
        int rc = udp_socket_read(server->socket_fd, &client_address, client_request, BUFFER_SIZE);

        // Successfully received an incoming request
        if (rc > 0)
        {
            // Print client's IP and port
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));

            // Demo code (remove later)
            strcpy(server_response, "Hi, the server has received: ");
            strcat(server_response, client_request);
            strcat(server_response, "\n");

            printf("%d: %s", ntohs(client_address.sin_port), client_request);

            // This function writes back to the incoming client,
            // whose address is now available in client_address, 
            // through the socket at sd.
            // (See details of the function in udp.h)
            rc = udp_socket_write(server->socket_fd, &client_address, server_response, BUFFER_SIZE);

        }
    }

}