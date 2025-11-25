#ifndef CHAT_STRUCTURES
#define CHAT_STRUCTURES

#include <pthread.h>
#include <netinet/in.h>

#define MAX_NAME_LEN 50
#define SERVER_PORT 12000
#define BUFFER_SIZE 1024


typedef struct ClientNode {
    char name[MAX_NAME_LEN];
    struct sockaddr_in address;
    int is_connected;
    struct ClientNode* next;
} ClientNode;


typedef struct MutedPair {
    char muter[MAX_NAME_LEN];      // Whos doing the muting
    char muted[MAX_NAME_LEN];      // Whos being muted
    struct MutedPair *next;          
} MutedPair;


typedef struct {
    ClientNode *ClientListHead;
    MutedPair *MutedHead;
    pthread_rwlock_t list_lock;
    pthread_rwlock_t mute_lock;
    int socket_fd;
} ServerContext;


typedef struct {
    ServerContext *server;
    char request[BUFFER_SIZE];
    struct sockaddr_in client_addr;
} WorkerArgs;

#endif