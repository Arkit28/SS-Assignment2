#include <stdio.h>
#include "udp.h"
#include <ctype.h>
#include "chat_parser.h" //for chat parsing
#include <stdlib.h>
#include <string.h>
#include <pthread.h>



//Client and server deatils shared between the 2 threads
int client_sd;//socket descriptor
struct sockaddr_in server_addr;//server address with IP and the PORT
char log_filename[64];//
volatile int keep_running = 1;//client running or shut down?


// Threads


//the listening thread from the server
void *receive_thread(void *arg)
{

}




//The thread sending information to the server
void *input_thread(void *arg)
{

}




// client code
int main(int argc, char *argv[])
{
    int admin_mode = (argc > 1 && strcmp(argv[1], "admin") == 0);

    //opening a UDP socket
    if (admin_mode) {
        //the admin mode uses port 6666
        client_sd = udp_socket_open(6666);
        printf("Admin client started on port 6666\n");
    } else {
        // normal cline so use any available port
        client_sd = udp_socket_open(0);
    }

    if (client_sd < 0) {
        perror("udp_socket_open failed");
        return 1;
    }

    //getting the local port we were assigned
    struct sockaddr_in self_addr;
    socklen_t addr_len = sizeof(self_addr);
    if (getsockname(client_sd, (struct sockaddr *)&self_addr, &addr_len) != 0) {
        perror("getsocket name failed");
        return 1;
    }
    unsigned short my_port = ntohs(self_addr.sin_port);
    printf("Client bound to UDP port %hu\n", my_port);



    //set the socket address of server
    if (set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT) != 0) {
        fprintf(stderr, "failed to initialise server address\n");
        return 1;
    }

    //building the filename for the current chat
    snprintf(log_filename, sizeof(log_filename), "iChat_%hu.txt", my_port);
    printf("Logging chat to %s\n", log_filename);


    //after all the setup is done we create the input and reciever threads
    pthread_t input_tid, recv_tid;
    pthread_create(&input_tid, NULL, input_thread, NULL);
    pthread_create(&recv_tid, NULL, receive_thread, NULL);


    //waiting for client to stop sending messages to server
    pthread_join(input_tid, NULL);


    //closing the reciever as client stops running
    keep_running = 0;
    pthread_join(recv_tid, NULL);


    close(client_sd);
    return 0;
}





