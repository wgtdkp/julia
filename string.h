#ifndef _JULIA_STRING_H_
#define _JULIA_STRING_H_

typedef struct {
    int len;
    char* data;
} string_t;

int print_string(const char* format, ...);

#endif
