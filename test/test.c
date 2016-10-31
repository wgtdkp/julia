#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/list.h"
#include "base/pool.h"
#include "base/queue.h"
#include "base/vector.h"
#include "base/heap.h"



int arr[101];

/*
int test_vector(void)
{
    for (int i = 0; i < 101; ++i) {
        arr[i] = i;
    }
    
    vector_t vec;
    vector_init(&vec, sizeof(int), 6);
    int* vec_data = vec.data;
    for (int i = 0; i < vec.size; ++i) {
        vec_data[i] = arr[i];
        printf("vec[%d]: %d\n", i, vec_data[i]);
    }
    printf("vec.size: %d\n", vec.size);
    printf("vec.capacity: %d\n", vec.capacity);
    
    vector_clear(&vec);
    for (int i = 0; i < 101; ++i) {
        *(int*)vector_push(&vec) = arr[i];
        //vector_push(&vec);
        printf("vec[%d]: %d\n", i, ((int*)vec.data)[i]);
    }
    
    while (vec.size > 0) {
        printf("back: %d\n", *(int*)vector_pop(&vec));
    }
    printf("vec.size: %d\n", vec.size);
    printf("vec.capacity: %d\n", vec.capacity);
    
    vector_clear(&vec);
    
    printf("vec.size: %d\n", vec.size);
    printf("vec.capacity: %d\n", vec.capacity);
    printf("vec.data: %p\n", vec.data);
    return 0;
}
*/
static int test_pool(void) {
    pool_t pool;
    pool_init(&pool, sizeof(int), 100, 0);
    
    int* arr[101];
    
    for (int i = 0; i < 101; ++i) {
        int* ele = pool_alloc(&pool);
        *ele = i;
        arr[i] = ele;
    }

    //printf("pool.nallocated: %d\n", pool.nallocated);
    //printf("chunck number: %d\n", pool.chunks.size);

    for (int i = 0; i < 101; ++i) {
        pool_free(&pool, arr[i]);
    }
    
    //printf("pool.nallocated: %d\n", pool.nallocated);
    //printf("chunck number: %d\n", pool.chunks.size);

    pool_clear(&pool);
    
    //printf("pool.nallocated: %d\n", pool.nallocated);
    //printf("chunck number: %d\n", pool.chunks.size);

    return 0;
}


static int test_list(void) {
    list_t list;
    pool_t pool;
    pool_init(&pool, LIST_WIDTH(int), 100, 0);
    list_init(&list, &pool);
    
    for (int i = 0; i < 101; ++i) {
        arr[i] = i;
    }
  
    for (int i = 0; i < 101; ++i) {
        list_node_t* new_node = list_alloc(&list);
        *(int*)&new_node->data = arr[i];
        list_insert(&list, &list.dummy, new_node);
    }


    list_node_t* p;
    for (p = list_head(&list); p != NULL; p = p->next) {
        int* ele = (int*)&p->data;
        printf("list ele: %d\n", *ele);
    }
    
    printf("list size: %d\n", list.size);
    printf("pool allocated: %d\n", list.pool->nallocated);
    
    list_delete(&list, list_head(&list)->next->next);
    list_delete(&list, list_tail(&list)->prev->prev);
    
    for (p = list_head(&list); p != NULL; p = p->next) {
        int* ele = (int*)&p->data;
        printf("list ele: %d\n", *ele);
    }
    
    list_clear(&list);
    
    printf("list size: %d\n", list.size);
    printf("pool allocated: %d\n", list.pool->nallocated);
    printf("list head: %p \n", list_head(&list));
    printf("list tail: %p \n", list_tail(&list));
    
    list_clear(&list);
    
    printf("list size: %d\n", list.size);
    printf("pool allocated: %d\n", list.pool->nallocated);
    printf("list head: %p \n", list_head(&list));
    printf("list tail: %p \n", list_tail(&list));
    
    return 0;
}



static int test_queue(void) {
    queue_t queue;
    pool_t pool;
    pool_init(&pool, QUEUE_WIDTH(int), 100, 0);
    queue_init(&queue, &pool);
    
    for (int i = 0; i < 101; ++i) {
        arr[i] = i;
    }
    
    for (int i = 0; i < 101; ++i) {
        int* ele = queue_alloc(&queue);
        *ele = arr[i];
        queue_push(&queue, ele);
    }
    
    //while (!queue_empty(&queue)) {
    //    int* ele = queue_front(&queue);
    //    //printf("ele: %d\n", *ele);
    //    queue_pop(&queue);
    //}
    
    //printf("queue size: %d\n", queue_size(&queue));
    
    queue_clear(&queue);
    
    //printf("queue size: %d\n", queue_size(&queue));
    
    return 0;
}

static int heap_comp(void* lhs, void* rhs) {
    return *(int*)lhs - *(int*)rhs;
}

static void print_heap(heap_t* h) {
    for (int i = 1; i <= heap_size(h); ++i) {
        int* v = heap_at(h, i);
        printf("%d ", *v);
    }
    printf("\n");
}

static int test_heap(void) {
    heap_t h;
    heap_init(&h, heap_comp);
    int arr[10] = {99, 23, 43, 45, 0, 9, 4, 8, 7, -1};
    int idx[10];
    for (int i = 0; i < 10; ++i) {
        idx[i] = heap_push(&h, &arr[i]);
    }

    for (int i = 0; i < 10; ++i) {
        int* v = heap_at(&h, idx[i]);
        *v = 200 + i;
        heap_shift_down(&h, idx[i]);
        print_heap(&h);
    }

    for (int i = 0; i < 10; ++i) {
        int* v = heap_pop(&h);
        printf("%d\n", *v);
    }

    return 0;
}


int main(void) {
    test_heap();
    //test_vector();
    //while (1)
    //    test_pool();
    //test_list();
    //while (1)
    //    test_queue();
    return 0;
}
