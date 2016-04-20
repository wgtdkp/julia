#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void print_string(String* str)
{
    printf("size: %d, capacity: %d, data: %s\n", str->size, str->capacity, str->data);
    fflush(stdout);
}

int main(void)
{
    String str = string_init(0);
    print_string(&str);
    char* p = "hello string";
    string_append(&str, p, strlen(p));
    print_string(&str);
    
    p = "111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    String str2 = string_init(0);
    string_append(&str2, p, strlen(p));

    String str3 = string_init(0);
    string_append(&str, str2.data, str2.size);
    print_string(&str);
    
    string_clear(&str);
    string_clear(&str2);
    string_clear(&str3);
    return 0;
}
