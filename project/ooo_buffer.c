#include "ooo_buffer.h"
#include <stdlib.h>
#include <string.h>

ooo_buffer* ooo_buffer_create(int capacity) {
    ooo_buffer* buf = malloc(sizeof(ooo_buffer));
    if (!buf)
        return NULL;
    buf->capacity = capacity;
    buf->entries = malloc(sizeof(out_of_order_entry) * capacity);
    if (!buf->entries) {
        free(buf);
        return NULL;
    }
    for (int i = 0; i < capacity; i++) {
        buf->entries[i].valid = 0;
        buf->entries[i].payload = NULL;
    }
    return buf;
}

void ooo_buffer_destroy(ooo_buffer* buf) {
    if (!buf)
        return;
    for (int i = 0; i < buf->capacity; i++) {
        if (buf->entries[i].valid && buf->entries[i].payload) {
            free(buf->entries[i].payload);
        }
    }
    free(buf->entries);
    free(buf);
}

// !!! expects length to already be ntohs(p->length) NOT raw p->length
void ooo_buffer_store(ooo_buffer* buf, int seq, int length, uint8_t *payload) {
    // Check for duplicate entry, if found, do nothing.
    for (int i = 0; i < buf->capacity; i++) {
        if (buf->entries[i].valid && buf->entries[i].seq == seq)
            return;
    }
    // Insert in the first available slot.
    for (int i = 0; i < buf->capacity; i++) {
        if (!buf->entries[i].valid) {
            buf->entries[i].valid = 1;
            buf->entries[i].seq = seq;
            buf->entries[i].length = length;
            buf->entries[i].payload = malloc(length);
            if (buf->entries[i].payload != NULL) {
                memcpy(buf->entries[i].payload, payload, length);
            }
            break;
        }
    }
}

int ooo_buffer_flush(ooo_buffer* buf, int *next_expected, void (*output_io)(uint8_t*, size_t)) {
    int total_flushed = 0;
    int found;
    do {
        found = 0;
        for (int i = 0; i < buf->capacity; i++) {
            if (buf->entries[i].valid && buf->entries[i].seq == *next_expected) {
                output_io(buf->entries[i].payload, buf->entries[i].length);
                *next_expected += buf->entries[i].length;
                total_flushed += buf->entries[i].length;
                free(buf->entries[i].payload);
                buf->entries[i].payload = NULL;
                buf->entries[i].valid = 0;
                found = 1;
                break; // Restart scan for the new *next_expected.
            }
        }
    } while(found);
    return total_flushed;
}