#include <stdio.h>
#include "udp.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// client and server details shared between the 2 threads
int client_sd;// socket descriptor
struct sockaddr_in server_addr;// server address with IP and the PORT
char log_filename[64];
volatile int keep_running = 1;// client running or shut down?
int admin_mode_activated = 0;


void *receive_thread(void *arg)
{
    char buffer[BUFFER_SIZE];// holds incoming UDP packet content
    struct sockaddr_in from_addr;// contains senders IP and port

    while (keep_running) {
        // waiting for a packet to be received and if so writing contents into the buffer
        int rc = udp_socket_read(client_sd, &from_addr, buffer, BUFFER_SIZE);

        if (rc <= 0) {
            continue;
        }

        // ensure null terminated string
        if (rc < BUFFER_SIZE) {
            buffer[rc] = '\0';
        } else {
            buffer[BUFFER_SIZE - 1] = '\0';
        }

        // server ping PE2
        if (strcmp(buffer, "ping$") == 0) {
            char reply[BUFFER_SIZE];
            strcpy(reply, "ret-ping$");
            udp_socket_write(client_sd, &server_addr, reply, strlen(reply) + 1);
            continue;
        }

        // handling server down message and kicked message
        if (strcmp(buffer, "server-down$") == 0 || strcmp(buffer, "kicked$") == 0) {
            printf("%s\n", buffer);
            fflush(stdout);
            keep_running = 0;
            break;
        }

        // display message in command terminal if a server message
        int is_server_message = (strncmp(buffer, "ERROR:", 6) == 0 ||
                                  strncmp(buffer, "Warning:", 8) == 0 ||
                                  strstr(buffer, "has joined") != NULL ||
                                  strstr(buffer, "has left") != NULL ||
                                  strncmp(buffer, "You have been", 13) == 0 ||
                                  strncmp(buffer, "Unexpected", 10) == 0 ||);

        if (is_server_message) {
            // server messages: display in terminal only
            printf("%s\n", buffer);
            fflush(stdout);
        } else {
            // chat messages: log to file only
            FILE *fp = fopen(log_filename, "a");
            if (fp != NULL) {
                fprintf(fp, "%s\n", buffer);
                fclose(fp);
            }
        }
    }

    return NULL;
}


void *input_thread(void *arg)
{
    char line[BUFFER_SIZE];

    while (keep_running) {
        printf(">> ");
        fflush(stdout);

        
        if (!fgets(line, sizeof(line), stdin)) {
            keep_running = 0;
            break;
        }

        //remove extra line 
        line[strcspn(line, "\n")] = '\0';

        
        if (line[0] == '\0')
            continue;

        // only admin can use kick$
        if (strncmp(line, "kick$", 5) == 0 && !admin_mode_activated) {
            printf("Kick request sent to admin.\n");
            fflush(stdout);

        }

        
        udp_socket_write(client_sd, &server_addr, line, strlen(line) + 1);

        // disconnect command done after sending command so client doesnt shut down before sending the disconn command to server
        if (strncmp(line, "disconn$", 8) == 0) {
            keep_running = 0;
            break;
        }
    }

    return NULL;
}


int main(int argc, char *argv[])
{
    admin_mode_activated = (argc > 1 && strcmp(argv[1], "admin") == 0);

    
    if (admin_mode_activated) {
        client_sd = udp_socket_open(6666);
        printf("Admin client started on port 6666\n");
    } else {
        client_sd = udp_socket_open(0);// any available port
    }

    if (client_sd < 0) {
        perror("udp_socket_open failed");
        return 1;
    }

    // get the local port we were assigned
    struct sockaddr_in self_addr;
    socklen_t addr_len = sizeof(self_addr);
    if (getsockname(client_sd, (struct sockaddr *)&self_addr, &addr_len) != 0) {
        perror("getsockname failed");
        return 1;
    }
    unsigned short my_port = ntohs(self_addr.sin_port);
    printf("Client bound to UDP port %hu\n", my_port);

    // set the socket address of server (localhost for now)
    if (set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT) != 0) {
        fprintf(stderr, "failed to initialise server address\n");
        return 1;
    }

    // build logfile name for this client instance
    snprintf(log_filename, sizeof(log_filename), "iChat_%hu.txt", my_port);
    printf("Logging chat to %s\n", log_filename);

    
    pthread_t input_tid, recv_tid;

    
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        perror("pthread_create for input thread failed");
        close(client_sd);
        return 1;
    }
    if (pthread_create(&recv_tid, NULL, receive_thread, NULL) != 0) {
        perror("pthread_create receive thread failed");
        keep_running = 0;
        pthread_join(input_tid, NULL);
        close(client_sd);
        return 1;
    }

    pthread_join(input_tid, NULL);

    keep_running = 0;
    pthread_join(recv_tid, NULL);

    close(client_sd);
    return 0;
}














