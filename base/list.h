#ifndef _JULIA_LIST_H_
#define _JULIA_LIST_H_

#include "pool.h"

#define LIST_WIDTH(type)    \
    2 * sizeof(list_node_t*) + sizeof(type)

typedef struct list_node list_node_t; 
struct list_node {
    list_node_t* next;
    list_node_t* prev;
    char data; // The minimum size
};

typedef struct {
    int size;
    list_node_t dummy;
    pool_t* pool;
} list_t;

int list_insert(list_t* list, list_node_t* pos, list_node_t* new_node);
int list_delete(list_t* list, list_node_t* x);

static inline list_node_t* list_head(list_t* list) {
    return list->dummy.next;
}

static inline list_node_t* list_tail(list_t* list) {
    return list->dummy.prev;
}

static inline int list_init(list_t* list, pool_t* pool) {
    list->size = 0;
    list->dummy.next = NULL;            // The head
    list->dummy.prev = &list->dummy;    // The tail
    list->pool = pool;
    return OK;
}

static inline void list_clear(list_t* list) {
    while (list_head(list) != NULL) {
        list_delete(list, list_head(list));
    }
}

static inline list_node_t* list_alloc(list_t* list) {
    return pool_alloc(list->pool);
}

static inline void list_free(list_t* list, list_node_t* x) {
    pool_free(list->pool, x);
}

#endif