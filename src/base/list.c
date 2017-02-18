#include "list.h"

#include <assert.h>

int list_insert(list_t* list, list_node_t* pos, list_node_t* new_node) {
    assert(pos != NULL);
    new_node->next = pos->next;
    if (new_node->next != NULL)
        new_node->next->prev = new_node;
    else
        list->dummy.prev = new_node;    // The tail
    new_node->prev = pos;
    pos->next = new_node;
    
    ++list->size;
    return OK;
}

int list_delete(list_t* list, list_node_t* x) {
    assert(list->size > 0 && x != &list->dummy);
    x->prev->next = x->next;
    if (x->next != NULL)
        x->next->prev = x->prev;
    else
        list->dummy.prev = x->prev; // The tail

    list_free(list, x);
    --list->size;
    return OK;
}
