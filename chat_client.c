
#include <stdio.h>
#include "udp.h"
#include <ctype.h>

#define CLIENT_PORT 10000

// client code
int main(int argc, char *argv[])
{
    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number CLIENT_PORT.
    // (See details of the function in udp.h)
    int sd = udp_socket_open(0);

    /* Retrieve and display the client's bound port number */
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(sd, (struct sockaddr *)&local_addr, &local_len) == 0)
    {
        printf("Client bound to port %d\n", ntohs(local_addr.sin_port));
    }
    else
    {
        perror("getsockname");
    }
    // Variable to store the server's IP address and port
    // (i.e. the server we are trying to contact).
    // Generally, it is possible for the responder to be
    // different from the server requested.
    // Although, in our case the responder will
    // always be the same as the server.
    struct sockaddr_in server_addr, responder_addr;

    // Initializing the server's address.
    // We are currently running the server on localhost (127.0.0.1).
    // You can change this to a different IP address
    // when running the server on a different machine.
    // (See details of the function in udp.h)
    int rc = set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT);
    while(1){
        // Storage for request and response messages
        char client_request[BUFFER_SIZE], server_response[BUFFER_SIZE];

        printf(">> ");
        fgets(client_request, BUFFER_SIZE, stdin);
        // Demo code (remove later)
        //strcpy(client_request, "hello");

        // This function writes to the server (sends request)
        // through the socket at sd.
        // (See details of the function in udp.h)
        rc = udp_socket_write(sd, &server_addr, client_request, BUFFER_SIZE);

        if (rc > 0)
        {
            // This function reads the response from the server
            // through the socket at sd.
            // In our case, responder_addr will simply be
            // the same as server_addr.
            // (See details of the function in udp.h)
            int rc = udp_socket_read(sd, &responder_addr, server_response, BUFFER_SIZE);

            // Demo code (remove later)
            printf("server_response: %s", server_response);
        }
    }

    return 0;
}