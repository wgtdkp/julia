#ifndef _JULIA_STRING_H
#define _JULIA_STRING_H

typedef struct {
    int capacity;
    int size;
    char* data;
} string_t;

void string_init(string_t* str, int c);
void string_clear(string_t* str);
void string_release(string_t* str);
void string_push_back(string_t* str, char ch);
void string_resize(string_t* str, unsigned new_size);
void string_copy(string_t* des, const char* p, int len);
void string_reserve(string_t* str, int c);
int string_append(string_t* str, const char* p, int len);
int string_print(string_t* str, const char* format, ...);

#endif
