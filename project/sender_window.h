#pragma once
#include "consts.h"  

typedef struct sender_window_node {
    struct sender_window_node* next;
    packet* p;
} sender_window_node;

typedef struct {
    int count;
    sender_window_node* head;
    sender_window_node* tail;
} sender_window_queue;

void init_sender_window_queue(sender_window_queue* q);
void enqueue_sender_window(sender_window_queue* q, packet* p);
packet* dequeue_sender_window(sender_window_queue* q);
packet* peek_sender_window(sender_window_queue* q);