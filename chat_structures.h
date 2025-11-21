#include <stdio.h>
#include <stdlib.h>

struct clientList {
    int IP;
    int port_id;
    char *nickname;
    
    struct clientList* next;

};