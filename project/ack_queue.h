#pragma once

#include <stdint.h>

typedef struct ack_node {
    int ack;
    struct ack_node* next;
} ack_node;

typedef struct {
    int count;
    ack_node* head;
    ack_node* tail;
} ack_queue;

// Initialize the ack queue
void init_ack_queue(ack_queue* q);

// enqueue an ack into the back of queue
void enqueue_ack(ack_queue* q, int ack);

// dequeue the first ack in the queue
int dequeue_ack(ack_queue* q);