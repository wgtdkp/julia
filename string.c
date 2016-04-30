#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <assert.h>

/*
 * string initiator with specific capacity c
 */


int print_string(const char* format, ...)
{
    int ret = 0;
    va_list args;
    va_start(args, format);
    const char* p;
    for (p = format; p[0] != 0; p++) {
        if (p[0] == '%' && p[1] == '*' && (p[2] == 's' || p[2] == 'S')) {
            char* str = va_arg(args, char*);
            int len = va_arg(args, int);
            for (int i = 0; i < len; i++)
                ret += printf("%c", str[i]);
            p += 2;
        } else {
            ret += printf("%c", p[0]);
        }
    }
    va_end(args);
    return ret;
}
