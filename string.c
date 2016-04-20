#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <assert.h>


static const String default_string = {
    .capacity = 0,
    .size = 0,
    .data = NULL
};

/*
 * string initiator with specific capacity c
 */
String string_init(int c)
{
    String str = default_string;
    if (c <= 0)
        return str;
    str.data = (char*)malloc(c * sizeof(char));
    // TODO(wgtdkp): handle malloc error
    assert(str.data != NULL);
    str.capacity = c;
    return str;
}

void string_release(String* str)
{
    str->size = 0;
    free(str->data);
    str->data = NULL;
}

void string_clear(String* str)
{
    string_resize(str, 0);
}

void string_push_back(String* str, char ch)
{
    if (str->size >= str->capacity - 1)
        string_reserve(str, str->capacity * 2);
    
    str->data[str->size++] = ch;
    str->data[str->size] = 0;
}

void string_resize(String* str, unsigned new_size)
{
    if (new_size + 1 > str->capacity)
        string_reserve(str, new_size + 1);
    if (new_size > str->size) {
        memset(str->data + str->size, 0, new_size - str->size);
        str->data[new_size] = 0;
        str->size = new_size;
    } else {
        str->data[new_size] = 0;
        str->size = new_size;
    }
}

void string_copy(String* des, const char* p, int len)
{
    string_reserve(des, len + 1);
    memcpy(des->data, p, len);
    des->data[len] = 0;
    des->size = len;
}

void string_reserve(String* str, int c)
{
    if (c <= str->capacity)
        return;
    char* new_data = (char*)malloc(c * sizeof(char));
    assert(new_data != NULL);
    if (str->size != 0) {
        memcpy(new_data, str->data, str->size);
        free(str->data);
    }
    new_data[str->size] = 0;
    str->capacity = c;
    str->data = new_data;
}

int string_append(String* str, const char* p, int len)
{
    string_reserve(str, str->size + len + 1);
    memcpy(str->data + str->size, p, len);
    str->data[str->size + len] = 0;
    str->size += len;
    return 0;
}

int string_print(String* str, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str->data, str->size, format, args);
    va_end(args);
    return ret;
}
