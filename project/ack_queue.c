#include "ack_queue.h"
#include <stdlib.h>

void init_ack_queue(ack_queue* q) {
    q->count = 0;
    q->head = NULL;
    q->tail = NULL;
}

void enqueue_ack(ack_queue* q, int ack) {
    ack_node* node = malloc(sizeof(ack_node));
    if (!node)
        return;
    node->ack = ack;
    node->next = NULL;
    if (q->head == NULL) {
        q->head = node;
        q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
    q->count++;
}

int dequeue_ack(ack_queue* q) {
    if (q->head == NULL)
        return -1;
    ack_node* node = q->head;
    int ack = node->ack;
    q->head = node->next;
    free(node);
    q->count--;
    return ack;
}