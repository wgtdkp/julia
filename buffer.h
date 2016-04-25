#ifndef _JULIA_BUFFER_H_
#define _JULIA_BUFFER_H_

#include "server.h"
#include <memory.h>

/*
 * This buffer is not an universal one.
 * It is designed for receiving request and sending response.
 * Some members and functions are designed to facilitate request 
 * parsing and nonblocking I/O model.
 */
typedef struct {
    // The current pos for retrieving data from buffer
    int cur;
    // The last position that the parser has successfully parsed the data,
    // it must be the end of a line in request header(i.e. '\n')
    int parsed_pos;
    // The size of the buffer
    int size;
    // We could use 'RECV_BUF_SIZE' directly, this is used for dynamic expanding
    // the buffer if we have to.
    // And we will never use 'RECV_BUF_SIZE' except for initializing 'capacity'.
    int capacity;
    char data[RECV_BUF_SIZE];
} Buffer;

static inline void buffer_init(Buffer* buffer)
{
    buffer->cur = -1;
    buffer->parsed_pos = -1;
    buffer->size = 0;
    buffer->capacity = RECV_BUF_SIZE;
}

// Get next char in the buffer.
// Return -1, if no more data in the buffer.
static inline int buffer_next(Buffer* buffer)
{
    if (buffer->cur + 1 >= buffer->size)
        return -1;
    return buffer->data[++buffer->cur];
}

static inline int buffer_peek(Buffer* buffer)
{
    if (buffer->cur + 1 >= buffer->size)
        return -1;
    return buffer->data[buffer->cur + 1];
}

static inline int buffer_append(Buffer* buffer, const char* data, int len)
{
    int appended_len = MIN(buffer->capacity - buffer->size, len);
    memcpy(buffer->data + buffer->size, data, appended_len);
    buffer->size += appended_len;
    return appended_len;
}

static inline void buffer_mark_parsed(Buffer* buffer)
{
    buffer->parsed_pos = buffer->cur;
}

// Discard the received data that has been successfully parsed.
// The remaining data is copied to the head of the buffer for next parsing.
static inline void buffer_discard_parsed(Buffer* buffer)
{
    int remained_begin = buffer->parsed_pos + 1;
    int remained_size = buffer->size - remained_begin + 1;
    // TODO(wgtdkp): Is it safe?
    memcpy(buffer->data, buffer->data + remained_begin, remained_size);
    buffer->cur = -1;
    buffer->parsed_pos = -1;
    buffer->size = remained_size;
}

#endif
