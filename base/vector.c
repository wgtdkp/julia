#include "vector.h"

#include "string.h"
#include "util.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>


int vector_init(vector_t* vec, int width, int size)
{
    assert(width != 0);
    if (size != 0) {
        vec->data = malloc(width * size);
        if (vec->data == NULL)
            return ERROR;
    } else {
        vec->data = NULL;
    }
    
    vec->width = width;
    vec->size = size;
    vec->capacity = size;
    return OK;
}

int vector_reserve(vector_t* vec, int c)
{   
    if (c != 0) {
        assert(vec->data == NULL);
        vec->data = malloc(vec->width * c);
        if (vec->data == NULL)
            return ERROR;
    }
    return OK;
}

int vector_resize(vector_t* vec, int new_size)
{
    if (new_size > vec->capacity) {
        int new_capacity = vec->capacity * 2 + 1;
        vec->data = realloc(vec->data, vec->width * new_capacity);
        if (vec->data == NULL)
            return ERROR;
        vec->capacity = new_capacity;
    }
    
    vec->size = new_size;
    return OK;
}

int vector_push(vector_t* vec, void* x)
{
    if (vector_resize(vec, vec->size + 1) != OK)
        return ERROR;
    void* pos = (uint8_t*)vec->data + (vec->size - 1) * vec->width;
    memcpy(pos, x, vec->width);
    return OK;
}

void* vector_pop(vector_t* vec)
{
    if (vec->size == 0)
        return NULL;
    --vec->size;
    
    return (void*)((uint8_t*)vec->data + vec->size * vec->width);
}

void vector_clear(vector_t* vec)
{
    vec->size = 0;
    vec->capacity = 0;
    free(vec->data);
    vec->data = NULL;
}
