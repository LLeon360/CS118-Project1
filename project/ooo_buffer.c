#include "ooo_buffer.h"
#include <stdlib.h>
#include <string.h>
#include "io.h"
#include "consts.h"
typedef struct bst_node {
    int seq;        // SEQ is used as key
    int length;      
    uint8_t *payload;    
    struct bst_node *left;
    struct bst_node *right;
} bst_node;

struct ooo_buffer {
    bst_node *root;    
    int capacity;      
    int size;        
};

// Recursively free all nodes in the BST
static void bst_destroy(bst_node *node) {
    if (!node) return;
    bst_destroy(node->left);
    bst_destroy(node->right);
    if(node->payload)
        free(node->payload);
    free(node);
}

static bst_node* bst_search(bst_node* root, int seq) {
    if (!root)
        return NULL;
    if (seq == root->seq)
        return root;
    else if (seq < root->seq)
        return bst_search(root->left, seq);
    else
        return bst_search(root->right, seq);
}

static bst_node* bst_create_node(int seq, int length, uint8_t *payload) {
    bst_node *node = malloc(sizeof(bst_node));
    if (!node) return NULL;
    node->seq = seq;
    node->length = length;
    node->payload = malloc(length);
    if (!node->payload) {
        free(node);
        return NULL;
    }
    memcpy(node->payload, payload, length);
    node->left = node->right = NULL;
    return node;
}

// recursively insert and returns new root (bc may change)
static bst_node* bst_insert(bst_node *root, int seq, int length, uint8_t *payload) {
    if (!root)
        return bst_create_node(seq, length, payload);
    if (seq == root->seq) {
        // Duplicate so do nothing
        return root;
    } else if (seq < root->seq) {
        root->left = bst_insert(root->left, seq, length, payload);
    } else {
        root->right = bst_insert(root->right, seq, length, payload);
    }
    return root;
}

static bst_node* bst_find_min(bst_node *node) {
    while (node && node->left)
        node = node->left;
    return node;
}

// recursive delete 
static bst_node* bst_delete(bst_node *root, int seq) {
    if (!root)
        return NULL;
    if (seq < root->seq) {
        root->left = bst_delete(root->left, seq);
    } else if (seq > root->seq) {
        root->right = bst_delete(root->right, seq);
    } else {
        // Found node to delete.
        if (!root->left) {
            bst_node *temp = root->right;
            if(root->payload)
                free(root->payload);
            free(root);
            return temp;
        } else if (!root->right) {
            bst_node *temp = root->left;
            if(root->payload)
                free(root->payload);
            free(root);
            return temp;
        } else {
            bst_node *temp = bst_find_min(root->right);
            // Copy temp's data into root.
            root->seq = temp->seq;
            root->length = temp->length;
            free(root->payload);
            root->payload = malloc(temp->length);
            if (root->payload)
                memcpy(root->payload, temp->payload, temp->length);
            // Delete the duplicate in the right subtree.
            root->right = bst_delete(root->right, temp->seq);
        }
    }
    return root;
}

// interface funcs

ooo_buffer* ooo_buffer_create(int capacity) {
    ooo_buffer *buf = malloc(sizeof(ooo_buffer));
    if (!buf)
        return NULL;
    buf->root = NULL;
    buf->capacity = capacity;
    buf->size = 0;
    return buf;
}

void ooo_buffer_destroy(ooo_buffer* buf) {
    if (!buf)
        return;
    bst_destroy(buf->root);
    free(buf);
}

void ooo_buffer_store(ooo_buffer* buf, int seq, int length, uint8_t *payload) {
    if (!buf || !payload)
        return;
    if (buf->size >= buf->capacity)
        return;
    // If duplicate exists, do nothing.
    if (bst_search(buf->root, seq))
        return;
    buf->root = bst_insert(buf->root, seq, length, payload);
    buf->size++;
}

int ooo_buffer_flush(ooo_buffer* buf, int *next_expected) {
    if (!buf || !next_expected)
        return 0;
    int flushed = 0;
    bst_node *target = NULL;
    // Continue searching for the node with key equal to *next_expected.
    while ((target = bst_search(buf->root, *next_expected)) != NULL) {
        // Output the payload.
        output_io(target->payload, target->length);
        flushed++;
        (*next_expected)++;
        // Delete the node from bst
        buf->root = bst_delete(buf->root, target->seq);
        buf->size--;
    }
    return flushed;
}

int ooo_buffer_is_full(ooo_buffer* buf) {
    if (!buf)
        return 0;
    return (buf->size >= buf->capacity);
}