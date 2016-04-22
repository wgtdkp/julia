#ifndef _JULIA_STRING_H
#define _JULIA_STRING_H

typedef struct {
    int capacity;
    int size;
    char* data;
} String;

void string_init(String* str, int c);
void string_clear(String* str);
void string_release(String* str);
void string_push_back(String* str, char ch);
void string_resize(String* str, unsigned new_size);
void string_copy(String* des, const char* p, int len);
void string_reserve(String* str, int c);
int string_append(String* str, const char* p, int len);
int string_print(String* str, const char* format, ...);

#endif
