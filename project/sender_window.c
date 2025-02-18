#include "sender_window.h"
#include <stdlib.h>

void init_sender_window_queue(sender_window_queue* q) {
    q->byte_count = 0;
    q->head = NULL;
    q->tail = NULL;
}

void enqueue_sender_window(sender_window_queue* q, packet* p) {
    sender_window_node* node = malloc(sizeof(sender_window_node));
    if (!node)
        return;
    node->p = p;
    node->next = NULL;
    if (q->head == NULL) {
        q->head = node;
        q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
    q->byte_count += ntohs(p->length);
}

packet* dequeue_sender_window(sender_window_queue* q) {
    if (q->head == NULL)
        return NULL;
    sender_window_node* node = q->head;
    packet* p = node->p;
    q->head = node->next;
    free(node);
    q->byte_count -= ntohs(p->length);
    return p;
}

packet* peek_sender_window(sender_window_queue* q) {
    if (q->head == NULL)
        return NULL;
    return q->head->p;
}

void destroy_sender_window_queue(sender_window_queue* q) {
    while (q->head != NULL) {
        dequeue_sender_window(q);
    }
    free(q);
}