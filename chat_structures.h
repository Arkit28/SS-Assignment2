#ifndef CHAT_STRUCTURES
#define CHAT_STRUCTURES

#include <pthread.h>
#include <netinet/in.h>

#define MAX_NAME_LEN 50
#define SERVER_PORT 12000
#define BUFFER_SIZE 1024
#define HISTORY_SIZE 15

enum {
    CONNECT = 0,
    DISCONNECT,
    MESSAGE,
    PRIVATE_MESSAGE,
    MUTE,
    UNMUTE,
    RENAME,
    KICK_REQUEST,    
};

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
    char messages[HISTORY_SIZE][BUFFER_SIZE];  // Circular buffer of messages
    int head;                                  // Next write position
    int count;                                 
    pthread_rwlock_t lock;                     
} MessageHistory;

typedef struct {
    ClientNode *ClientListHead;
    MutedPair *MutedListHead;
    MessageHistory history;
    pthread_rwlock_t client_list_lock;
    pthread_rwlock_t mute_list_lock;
    int socket_fd;
} ServerContext;


typedef struct {
    ServerContext *server;
    char request[BUFFER_SIZE];
    struct sockaddr_in client_addr;
} WorkerArgs;

#endif