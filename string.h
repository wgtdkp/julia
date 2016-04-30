#ifndef _JULIA_STRING_H_
#define _JULIA_STRING_H_

#include <stdlib.h>

typedef struct {
    char* begin;
    char* end;
} string_t;

static inline void string_init(string_t* str)
{
    str->begin = NULL;
    str->end = NULL;
}

int print_string(const char* format, ...);

#endif
