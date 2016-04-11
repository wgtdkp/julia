#ifndef _JULIA_BUFFER_H_
#define _JULIA_BUFFER_H_

typedef struct {
    int cap;
    int size;
    char* data;
} Buffer;

Buffer* create_buffer(int c);
Buffer* destroy_buffer(Buffer** buf);
void buffer_push_back(Buffer* buf, char ch);

#endif
