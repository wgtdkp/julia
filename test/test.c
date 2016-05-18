#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/vector.h"
#include "base/pool.h"

int test_vector(void)
{
    vector_t vec;
    vector_init(&vec, sizeof(int), 0);
    for (int i = 0; i < 100; i++)
        vector_push(&vec, &i);
    for (int i = 0; i < vec.size; i++) {
        int* ele = vector_at(&vec, i);
        printf("%d ", *ele);
    }
    printf("\nvector size: %d\n", vec.size);
    vector_resize(&vec, 88);
    printf("size: %d capacity: %d\n", vec.size, vec.capacity);
    vector_resize(&vec, 180);
    printf("size: %d capacity: %d\n", vec.size, vec.capacity);
    vector_clear(&vec);
    printf("size: %d capacity: %d\n", vec.size, vec.capacity);
    //while (vec.size > 0) {
    //    int* back = vector_back(&vec);
    //    printf("vector back: %d\n", *back);
    //    assert(back == vector_pop(&vec));
    //}

    return 0;
}

int test_pool(void)
{
    pool_t pool;
    vector_t vec;
    vector_init(&vec, sizeof(int*), 0);
    pool_init(&pool, sizeof(int), 100, 1);
    for (int i = 0; i < 101; i++) {
        int* ele = pool_alloc(&pool);
        printf("cur: %p\n", pool.cur);
        *ele = i;
        vector_push(&vec, &ele);
    }
    printf("chunk number: %d\n", pool.chunks.size);
    printf("allocated: %d\n", pool.nallocated);
    
    for (int i = 0; i < 101; i++) {
        int* ele = *(int**)vector_at(&vec, i);
        pool_free(&pool, ele);
        printf("cur: %p\n", pool.cur);
    }
    
    printf("chunk number: %d\n", pool.chunks.size);
    printf("allocated: %d\n", pool.nallocated);
    
    pool_clear(&pool); 
    printf("chunk number: %d\n", pool.chunks.size);
    printf("allocated: %d\n", pool.nallocated);

    return 0;
}


int main(void)
{
    //test_vector();
    test_pool();
    return 0;
}
