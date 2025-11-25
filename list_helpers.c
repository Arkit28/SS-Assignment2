#include "list_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


//client managing functions

//add a new client to the list (assumes WRITE lock is held)
void list_add_client(ClientNode **head, char *name, struct sockaddr_in *address){
    if (head == NULL || name == NULL || address == NULL) return;

    ClientNode* new_node = (ClientNode*) malloc(sizeof(ClientNode));
    if (new_node == NULL) return;   // memory allocation failed

    strncpy(new_node->name, name, MAX_NAME_LEN - 1);
    new_node->name[MAX_NAME_LEN - 1] = '\0';

    new_node->address = *address;
    new_node->is_connected = 1;

    new_node->next = *head;
    *head = new_node;
}


// remove a clientnode from the list(assumes WRITE lock is held)
int list_remove_client(ClientNode **head, struct sockaddr_in *address){
    if (head == NULL || *head == NULL || address == NULL) return 0;

    ClientNode* current = *head;
    ClientNode* previous = NULL;

    while (current != NULL) {
        if (addresses_equal(&current->address, address)) {
            if (previous == NULL) {
                *head = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return 1;       //user removed
        }
        previous = current;
        current = current->next;
    }
    return 0;       //user not found
}

// find client using their address (assumes lock is held - READ or WRITE)
ClientNode* list_find_by_address(ClientNode *head, struct sockaddr_in *address){
    ClientNode* current = head;

    while(current != NULL){
        if(addresses_equal(&current->address, address)){
            return current;
        }
        current = current->next;
    }

    return NULL;
}


// Find client by name (assumes lock is held - READ or WRITE)
ClientNode* list_find_by_name(ClientNode *head, char *name){
    ClientNode* current = head;
    
    while(current != NULL){
        if(strcmp(current->name, name) == 0){
            return current;
        }
        current = current->next;
    }
    return NULL;
}


// check if address is same 
int addresses_equal(struct sockaddr_in *a, struct sockaddr_in *b){
    if (a == NULL || b == NULL) return 0;
    return (a->sin_family == b->sin_family) &&
           (a->sin_addr.s_addr == b->sin_addr.s_addr) &&
           (a->sin_port == b->sin_port);

}

//print entire list for debugging (assumes READ lock is held)
void list_print_all(ClientNode *head){
    ClientNode* current = head;

    inet_ntop(AF_INET, &head->address.sin_addr, NULL, 0);
    while(current != NULL){
        printf("Client Name: %s, IP: %s, Port: %d, Connected: %d\n", 
               current->name, 
               inet_ntoa(current->address.sin_addr), 
               ntohs(current->address.sin_port), 
               current->is_connected);

        current = current->next;
    }
        
}

// delete whole list(assumes WRITE lock is held)
void list_free_all(ClientNode **head){
    ClientNode* current = *head;
    ClientNode* next_node;

    while(current != NULL){
        next_node = current->next;
        free(current);
        current = next_node;
    }

    *head = NULL;
}




// muted ppl functions

// Add a muted person to clinet's list (assumes WRITE lock on mute_lock is held)
void mute_add(MutedPair **head, char *muter, char *muted){

    if(is_muted(*head, muter, muted)){
        printf("User %s has already muted %s\n", muter, muted);
        return;
    }

    MutedPair* new_pair = (MutedPair*) malloc(sizeof(MutedPair));
    if(new_pair == NULL) return;

    strncpy(new_pair->muter, muterm MAX_NAME_LEN-1);
    new_pair->muter[MAX_NAME_LEN-1] = '\0';

    strncpy(new_pair->muted, muted);
    new_pair->muted[MAX_LEN_NAME-1] = '\0';

    new_pair->next = *head;
    *head = new_pair;

    printf("User %s has muted %s\n", muter, muted);
}

//adds specific person to all clients' muted lists
void server_mute(MutedPair **head){}

// Check if client has muted the specified person (assumes READ lock is held)
int is_muted(MutedPair *head, char *muter, char *muted){
    MutedPair* current = head;

    while(current != NULL){
        if(strcmp(current->muter, muter) == 0 && strcmp(current->muted, muted) == 0){
            return 1;
        }
        current = current->next;
    }

    return 0;
}

// unmute a specific person (assumes WRITE lock is held)
int mute_remove(MutedPair **head, char *muter, char *muted){
    MutedPair* current = *head;
    MutedPair* previous = NULL;

    while(current != NULL){
        if(strcmp(current->muter, muter) == 0 && strcmp(current->muted, muted) == 0){
            if(prev == NULL){
                *head = current->next;
            }
            else{
                previous->next = current->next;
            }

            printf("%s unmuted %s\n", muter, muted);
            free(current);
            return 1;       //unmuted successfully
        }
        prev = curr;
        current = current->next;
    }

    return 0;   //mute pairing not found

}

// remove all muted ppl for a client (assumes WRITE lock is held)
// Used when a client disconnects
void mute_remove_all_for_client(MutedPair **head, char *client_name){
    MutedPair* current = *head;
    MutedPair* previous = NULL;

    while(current != NULL){
        if(strcmp(current->muter, client_name) == 0 || strcmp(current->muted, client_name) == 0){
            if(previous == NULL){
                *head = current->next;
                free(current);
                current = *head;
            }
            else{
                previous = current;
                free(current);
                current = previous->next;
            }
        }
    }
}

// print all muted list for every person (assumes READ lock is held)
void mute_print_all(MutedPair *head){}

// delete muted ppl list (assumes WRITE lock is held)
void mute_free_all(MutedPair **head){}


