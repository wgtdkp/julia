#include "buffer.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

Buffer* create_buffer(int c)
{
    Buffer* buf = (Buffer*)malloc(sizeof(Buffer));
    buf.size = 0;
    buf.data = (char*)malloc(c * sizeof(char));
    buf.cap = c;
    return buf;
}

Buffer* destroy_buffer(Buffer** buf)
{
    free((*buf)->data);
    free(*buf);
    *buf = 0;
}

void buffer_push_back(Buffer* buf, char ch)
{
    if (buf.size >= buf.cap - 1) {
        int new_cap = buf.cap * 2;
        char* new_data = (char*)malloc(new_cap * sizeof(char));
        memcpy(new_data, buf.data, buf.size);
        new_data[buf.size++] = ch;
        new_data[buf.size] = 0;
        free(buf.data);
        buf.data = new_data;
        buf.cap = new_cap;
        ++buf.size;
    } else {
        buf.data[buf.size++] = ch;
        buf.data[buf.size] = 0;
    }
}
