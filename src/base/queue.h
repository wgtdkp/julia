#ifndef _JULIA_QUEUE_H_
#define _JULIA_QUEUE_H_

#include "list.h"
#include "pool.h"

#define QUEUE_WIDTH(type)   LIST_WIDTH(type) 


typedef list_node_t queue_node_t;

typedef struct {
    list_t container;
} queue_t;

static inline int queue_init(queue_t* queue, pool_t* pool) {
    return list_init(&queue->container, pool);
}

static inline void queue_clear(queue_t* queue) {
    list_clear(&queue->container);
}

static inline void* queue_alloc(queue_t* queue) {
    return (void*)(&list_alloc(&queue->container)->data);
}

static inline int queue_push(queue_t* queue, void* x) {
    list_t* list = &queue->container;
    void* node_x = (char*)x - sizeof(list_node_t*) * 2;
    return list_insert(list, list_tail(list), node_x);
}

static inline void queue_pop(queue_t* queue) {
    list_t* list = &queue->container;
    list_delete(list, list_head(list));
}

static inline int queue_size(queue_t* queue) {
    return queue->container.size;
}

static inline int queue_empty(queue_t* queue) {
    return queue_size(queue) == 0;
}

static inline void* queue_front(queue_t* queue) {
    list_t* list = &queue->container;
    list_node_t* head = list_head(list);
    if (head == NULL)
        return NULL;
    return &head->data;
}

static inline void* queue_back(queue_t* queue) {
    list_t* list = &queue->container;
    list_node_t* tail = list_tail(list);
    if (tail == &list->dummy)
        return NULL;
    return &tail->data;
}

#endif
