#ifndef SERVER_LIST_H
#define SERVER_LIST_H

#include "chat_structures.h"

//client managing functions

//add a new client to the list (assumes WRITE lock is held)
void list_add_client(ClientNode **head, char *name, struct sockaddr_in *address);

// remove a clientnode from the list(assumes WRITE lock is held)
int list_remove_client(ClientNode **head, struct sockaddr_in *address);

// find client using their address (assumes lock is held - READ or WRITE)
ClientNode* list_find_by_address(ClientNode *head, struct sockaddr_in *address);

// Find client by name (assumes lock is held - READ or WRITE)
ClientNode* list_find_by_name(ClientNode *head, char *name);

// check if address is same 
int addresses_equal(struct sockaddr_in *a, struct sockaddr_in *b);

//print entire list for debugging (assumes READ lock is held)
void list_print_all(ClientNode *head);

// delete whole list(assumes WRITE lock is held)
void list_free_all(ClientNode **head);




// muted ppl functions

// Add a muted person to clinet's list (assumes WRITE lock on mute_lock is held)
void mute_add(MutedPair **head, char *muter, char *muted);

// Check if client has muted the specified person (assumes READ lock is held)
int is_muted(MutedPair *head, char *muter, char *muted);

// unmute a specific person (assumes WRITE lock is held)
int mute_remove(MutedPair **head, char *muter, char *muted);

// remove all muted ppl for a client (assumes WRITE lock is held)
// Used when a client disconnects
void mute_remove_all_for_client(MutedPair **head, char *client_name);

// print all muted list for every person (assumes READ lock is held)
void mute_print_all(MutedPair *head);

// delete muted ppl list (assumes WRITE lock is held)
void mute_free_all(MutedPair **head);

#endif