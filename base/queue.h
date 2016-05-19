#ifndef _JULIA_QUEUE_H_
#define _JULIA_QUEUE_H_

#include "list.h"
#include "pool.h"

typedef list_node_t queue_node_t;

typedef struct {
    list_t container;
} queue_t;

static inline int queue_init(queue_t* queue, pool_t* pool)
{
    list_init(&queue->container, pool);
    return OK;
}

static inline void queue_clear(queue_t* queue)
{
    list_clear(&queue->container);
}

static inline int queue_push(queue_t* queue, void* x)
{
    list_t* list = &queue->container;
    return list_insert(list, list_tail(list), x);
}

static inline void* queue_pop(queue_t* queue)
{
    void* ret;
    list_t* list = &queue->container;
    list_node_t* head = list_head(list);
    ret = head == NULL ? NULL: head->data;
    list_delete(list, head);
    return ret;
}

static inline int queue_size(queue_t* queue)
{
    return queue->container.size;
}

static inline int queue_empty(queue_t* queue)
{
    return queue_size(queue) == 0;
}

static inline void* queue_front(queue_t* queue)
{
    list_t* list = &queue->container;
    list_node_t* head = list_head(list);
    if (head == NULL)
        return NULL;
    return head->data;
}

static inline void* queue_back(queue_t* queue)
{
    list_t* list = &queue->container;
    list_node_t* tail = list_tail(list);
    if (tail == &list->dummy)
        return NULL;
    return tail->data;
}

#endif
