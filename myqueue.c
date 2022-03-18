#include "myqueue.h"
#include <stdlib.h>

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(struct serverInfo *info) {
    node_t *new_node = malloc(sizeof(node_t));
    new_node->info = info; 
    new_node->next = NULL;
    if (tail == NULL) {
        head = new_node;
    } else {
        tail->next = new_node;
    }
    tail = new_node;
}

// returns NULL if queue is empty
struct serverInfo* dequeue() {
    if (head == NULL) {
        return NULL;
    } else {
        struct serverInfo* info = head->info;
        node_t *temp = head;
        head = head->next;
        if (head == NULL) tail = NULL;
        free(temp);
        return info;
    }
}

