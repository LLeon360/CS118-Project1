#include "ooo_buffer.h"
#include <stdlib.h>
#include <string.h>
#include "io.h"

ooo_buffer* ooo_buffer_create(int capacity) {
    ooo_buffer* buf = malloc(sizeof(ooo_buffer));
    if (!buf)
        return NULL;
    buf->capacity = capacity;
    buf->entries = calloc(capacity, sizeof(out_of_order_entry));
    if (!buf->entries) {
        free(buf);
        return NULL;
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

int ooo_buffer_flush(ooo_buffer* buf, int *next_expected) {
    int total_flushed = 0;
    int found;
    do {
        found = 0;
        for (int i = 0; i < buf->capacity; i++) {
            if (buf->entries[i].valid && buf->entries[i].seq == *next_expected) {
                output_io(buf->entries[i].payload, buf->entries[i].length);
                *next_expected += 1;
                total_flushed += 1;
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

int ooo_buffer_is_full(ooo_buffer* buf) {
    int count = 0;
    for (int i = 0; i < buf->capacity; i++) {
        if (buf->entries[i].valid)
            count++;
    }
    return count == buf->capacity;
}