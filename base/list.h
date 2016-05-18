#ifndef _JULIA_LIST_H_
#define _JULIA_LIST_H_

#include "pool.h"

typedef struct list_node list_node_t; 
struct list_node {
    void* data;
    list_node_t* next;
    list_node_t* prev;
};

typedef struct {
    int size;
    list_node_t dummy;
    pool_t* pool;
} list_t;

int list_insert(list_t* list, list_node_t* pos, void* x);
int list_delete(list_t* list, list_node_t* x);

static inline int list_init(list_t* list, pool_t* pool)
{
    list->size = 0;
    list->dummy.data = NULL;
    list->dummy.next = NULL;
    list->dummy.prev = &list->dummy;    // The tail
    list->pool = pool;
    return OK;
}

static inline void list_clear(list_t* list)
{
    while (list->size > 0) {
        list_delete(list, list->dummy.next);
    }
}

#endif