#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/vector.h"
#include "base/pool.h"

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

    return 0;
}


int main(void)
{
    //test_vector();
    test_pool();
    return 0;
}
