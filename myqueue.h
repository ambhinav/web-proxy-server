#ifndef MYQUEUE_H_
#define MYQUEUE_H_

struct node {
    struct node* next;
    struct serverInfo* info;
};

typedef struct node node_t;

void enqueue(struct serverInfo *info);

struct serverInfo* dequeue();

#endif