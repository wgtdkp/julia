#ifndef _JULIA_STRING_H_
#define _JULIA_STRING_H_

#include <stdlib.h>
#include <string.h>


typedef struct {
    char* begin;
    char* end;
} string_t;

static inline void string_init(string_t* str)
{
    str->begin = NULL;
    str->end = NULL;
}

/*
 *  Initiate a string_t with zero terminated char stream
 */

static inline string_t string_settol(char* cstr, int len)
{
    return (string_t){cstr, cstr + len};
}

static inline string_t string_setto(char* cstr)
{
    return string_settol(cstr, strlen(cstr));
}



int print_string(const char* format, ...);
int string_cmp(string_t* lhs, string_t* rhs);

#endif
