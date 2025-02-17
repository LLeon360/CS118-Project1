#pragma once
#include <stdint.h>
#include <stddef.h>

#define DEFAULT_OUT_OF_ORDER_CAPACITY 40

typedef struct ooo_buffer ooo_buffer;

// Create a new out-of-order buffer with the specified capacity.
// Returns a pointer to the new buffer, or NULL on failure.
ooo_buffer* ooo_buffer_create(int capacity);

// Destroy a buffer and free all associated memory.
void ooo_buffer_destroy(ooo_buffer* buf);

// Insert an out-of-order packet into the buffer.
// If a packet with the same seq already exists, it does nothing.
void ooo_buffer_store(ooo_buffer* buf, int seq, int length, uint8_t *payload);

// Flush contiguous packets starting with seq number *next_expected.
// For every stored packet with seq matching *next_expected, output its payload,
//   update *next_expected, and free that entry.
// Returns the total number of packets flushed.
int ooo_buffer_flush(ooo_buffer* buf, int *next_expected);

int ooo_buffer_is_full(ooo_buffer* buf);