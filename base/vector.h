#ifndef _JULIA_VECTOR_H_
#define _JULIA_VECTOR_H_

#include "util.h"

#include <stdint.h>
#include <stdlib.h>


typedef struct {
    int width;  // Sizeof Element
    int size;
    int capacity;
    void* data;
} vector_t;

int vector_init(vector_t* vec, int width, int size);
int vector_reserve(vector_t* vec, int c);
int vector_resize(vector_t* vec, int new_size);
int vector_push(vector_t* vec, void* x);
void* vector_pop(vector_t* vec);
void vector_clear(vector_t* vec);

static inline void* vector_at(vector_t* vec, int i)
{
    return (void*)((uint8_t*)vec->data + i * vec->width);
}

static inline void* vector_back(vector_t* vec)
{
    return vector_at(vec, vec->size - 1);
}

static inline void* vector_alloc(vector_t* vec)
{
    if (vector_resize(vec, vec->size + 1) != OK)
        return NULL;
    return vector_back(vec);
}

#endif
