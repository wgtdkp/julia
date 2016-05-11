#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <assert.h>

// '%*s' <- string_t 
int print_string(const char* format, ...)
{
    int ret = 0;
    va_list args;
    va_start(args, format);
    const char* p;
    for (p = format; p[0] != 0; p++) {
        if (p[0] == '%' && p[1] == '*' && (p[2] == 's' || p[2] == 'S')) {
            string_t str = va_arg(args, string_t);
            for (char* q = str.begin; q < str.end; q++)
                ret += printf("%c", *q);
            p += 2;
        } else {
            ret += printf("%c", p[0]);
        }
    }
    va_end(args);
    return ret;
}

int string_cmp(string_t* lhs, string_t* rhs)
{
    if (lhs->begin == rhs->begin && lhs->end == rhs->end)
        return 0;
    if (lhs->begin == NULL)
        return -1;
    if (rhs->begin == NULL)
        return 1;
    char* pl = lhs->begin;
    char* pr = rhs->begin;
    for (; pl < lhs->end && pr < rhs->end; pl++, pr++) {
        if (*pl < *pr)
            return -1;
        else if (*pl > *pr)
            return 1;
    }
    if (pl == lhs->end && pr == rhs->end)
        return 0;
    return (pl == lhs->end) ? -1: 1;
}

bool string_eq(string_t* lhs, string_t* rhs)
{
    if (lhs->begin == NULL && rhs->begin == NULL)
        return true;
    if (lhs->begin == NULL || rhs->begin == NULL)
        return false;
    if (lhs->end - lhs->begin != rhs->end - rhs->begin)
        return false;
    char* pl = lhs->begin;
    char* pr = rhs->begin;
    for (; pl < lhs->end; pl++) {
        if (*pl != *pr)
            return false;
        pr++;
    }
    return true;
}
