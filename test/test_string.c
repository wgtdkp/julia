#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void print_string(string_t* str)
{
    printf("size: %d, capacity: %d, data: %s\n", str->size, str->capacity, str->data);
    fflush(stdout);
}

int main(void)
{
    string_t str;
    string_init(&str, 0);
    print_string(&str);
    char* p = "hello string";
    string_append(&str, p, strlen(p));
    print_string(&str);
    
    p = "111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    string_t str2;
    string_init(&str2, 0);
    string_append(&str2, p, strlen(p));

    string_t str3;
    string_init(&str3, 0);
    string_append(&str, str2.data, str2.size);
    print_string(&str);
    
    string_clear(&str);
    string_clear(&str2);
    string_clear(&str3);
    return 0;
}
