#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <assert.h>

// '%*s' <- string_t*
int print_string(const char* format, ...) {
    int ret = 0;
    va_list args;
    va_start(args, format);
    const char* p;
    for (p = format; p[0] != 0; ++p) {
        if (p[0] == '%' && p[1] == '*' && (p[2] == 's' || p[2] == 'S')) {
            string_t* str = va_arg(args, string_t*);
            for (int i = 0; i < str->len; ++i)
                ret += printf("%c", str->data[i]);
            p += 2;
        } else {
            ret += printf("%c", p[0]);
        }
    }
    va_end(args);
    return ret;
}

int string_cmp(const string_t* lhs, const string_t* rhs) {
    if (lhs->data == rhs->data && lhs->len == rhs->len)
        return 0;
    if (lhs->data == NULL)
        return -1;
    if (rhs->data == NULL)
        return 1;

    int i = 0, j = 0;
    for (; i < lhs->len && j < rhs->len; i++, ++j) {
        if (lhs->data[i] < rhs->data[j])
            return -1;
        else if (lhs->data[i] > rhs->data[j])
            return 1;
    }
    
    if (i == lhs->len && j == rhs->len)
        return 0;
    return (i == lhs->len) ? -1: 1;
}

bool string_eq(const string_t* lhs, const string_t* rhs) {
    if (lhs->data == NULL && rhs->data == NULL)
        return true;
    if (lhs->data == NULL || rhs->data == NULL)
        return false;
    if (lhs->len != rhs->len)
        return false;
        
    for (int i = 0; i < lhs->len; ++i) {
        if (lhs->data[i] != rhs->data[i])
            return false;
    }
    return true;
}
