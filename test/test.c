#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/list.h"
#include "base/pool.h"
#include "base/queue.h"
#include "base/vector.h"

int arr[101];

int test_vector(void)
{
    for (int i = 0; i < 101; i++) {
        arr[i] = i;
    }
    
    vector_t vec;
    vector_init(&vec, 6);
    for (int i = 0; i < vec.size; i++) {
        vec.data[i] = &arr[i];
        printf("vec[%d]: %d\n", i, *(int*)vec.data[i]);
    }
    printf("vec.size: %d\n", vec.size);
    printf("vec.capacity: %d\n", vec.capacity);
    
    vector_clear(&vec);
    for (int i = 0; i < 101; i++) {
        vector_push(&vec, &arr[i]);
        printf("vec[%d]: %d\n", i, *(int*)vec.data[i]);
    }
    
    while (vec.size > 0) {
        printf("back: %d\n", *(int*)vector_pop(&vec));
    }
    printf("vec.size: %d\n", vec.size);
    printf("vec.capacity: %d\n", vec.capacity);
       
    return 0;
}

int test_pool(void)
{
    pool_t pool;
    vector_t vec;
    vector_init(&vec, 0);
    pool_init(&pool, sizeof(int), 101, 1);
    
    for (int i = 0; i < 101; i++) {
        int* ele = pool_alloc(&pool);
        *ele = i;
        vector_push(&vec, ele);
    }
    
    printf("pool.nallocated: %d\n", pool.nallocated);
    printf("chunck number: %d\n", pool.chunks.size);
    while (vec.size > 0) {
        int* ele = vector_pop(&vec);
        printf("back: %d\n", *ele);
        pool_free(&pool, ele);
    }
    
    printf("pool.nallocated: %d\n", pool.nallocated);
    printf("chunck number: %d\n", pool.chunks.size);

    pool_clear(&pool);
    return 0;
}

int test_list(void)
{
    pool_t pool;
    pool_init(&pool, sizeof(list_node_t), 100, 0);
    list_t list;
    list_init(&list, &pool);
    
    for (int i = 0; i < 101; i++) {
        arr[i] = i;
    }
  
    for (int i = 0; i < 101; i++) {
        list_insert(&list, &list.dummy, &arr[i]);
    }


    list_node_t* p;
    for (p = list_head(&list); p != NULL; p = p->next) {
        int* ele = p->data;
        printf("list ele: %d\n", *ele);
    }
    
    printf("list size: %d\n", list.size);
    printf("pool allocated: %d\n", pool.nallocated);
    
    list_delete(&list, list_head(&list)->next->next);
    list_delete(&list, list_tail(&list)->prev->prev);
    
    for (p = list_head(&list); p != NULL; p = p->next) {
        int* ele = p->data;
        printf("list ele: %d\n", *ele);
    }
    
    //while (list_head(&list) != NULL) {
    //    printf("%d \n", *(int*)list_tail(&list)->data);
    //    list_delete(&list, list_tail(&list));
    //}
    
    list_clear(&list);
    
    printf("list size: %d\n", list.size);
    printf("pool allocated: %d\n", pool.nallocated);
    printf("list head: %p \n", list_head(&list));
    printf("list tail: %p \n", list_tail(&list));
    
    pool_clear(&pool);
    return 0;
}

int test_queue(void)
{
    pool_t pool;
    pool_init(&pool, sizeof(list_node_t), 100, 0);
    queue_t queue;
    queue_init(&queue, &pool);
    
    for (int i = 0; i < 101; i++) {
        arr[i] = i;
    }
    
    for (int i = 0; i < 101; i++) {
        queue_push(&queue, &arr[i]);
    }
    
    while (!queue_empty(&queue)) {
        int* ele = queue_pop(&queue);
        printf("ele: %d\n", *ele);
    }
    
    printf("queue size: %d\n", queue_size(&queue));
    
    return 0;
}


int main(void)
{
    //test_vector();
    test_pool();
    //test_list();
    //test_queue();
    return 0;
}
