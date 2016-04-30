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
