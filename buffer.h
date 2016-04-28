#ifndef _JULIA_BUFFER_H_
#define _JULIA_BUFFER_H_

#include "server.h"
#include "util.h"

#include <memory.h>
#include <stdbool.h>

/*
 * This buffer is not an universal one.
 * It is designed for receiving request and sending response.
 * Some members and functions are designed to facilitate request 
 * parsing and nonblocking I/O model.
 */
typedef struct {
    // The cursor for retrieving data from buffer
    int cur;
    // The last position that the parser has successfully parsed the data,
    // it must be the next char of '\n'
    int parsed;
    // The size of the buffer
    int size;
    // We could use 'RECV_BUF_SIZE' directly, this is used for dynamic expanding
    // the buffer if we have to.
    // And we will never use 'RECV_BUF_SIZE' except for initializing 'capacity'.
    int capacity;
    char data[RECV_BUF_SIZE];
} buffer_t;

static inline void buffer_init(buffer_t* buffer)
{
    buffer->cur = 0;
    buffer->parsed = 0;
    buffer->size = 0;
    buffer->capacity = RECV_BUF_SIZE;
}

// Get next char in the buffer.
// Return -1, if no more data in the buffer.
static inline int buffer_next(buffer_t* buffer)
{
    if (buffer->cur >= buffer->size)
        return -1;
    return buffer->data[buffer->cur++];
}

static inline int buffer_peek(buffer_t* buffer)
{
    if (buffer->cur >= buffer->size)
        return -1;
    return buffer->data[buffer->cur];
}

static inline int buffer_append(buffer_t* buffer, const char* data, int len)
{
    int appended_len = min(buffer->capacity - buffer->size, len);
    memcpy(buffer->data + buffer->size, data, appended_len);
    buffer->size += appended_len;
    return appended_len;
}

static inline void buffer_mark_parsed(buffer_t* buffer)
{
    buffer->parsed = buffer->cur;
}

// Discard the received data that has been successfully parsed.
// The remaining data is copied to the head of the buffer for next parsing.
static inline void buffer_discard_parsed(buffer_t* buffer)
{
    int remained_begin = buffer->parsed;
    int remained_size = buffer->size - remained_begin;
    // TODO(wgtdkp): Is it safe?
    // There shouldn't be much data remained
    memcpy(buffer->data, buffer->data + remained_begin, remained_size);
    buffer->cur = 0;
    buffer->parsed = 0;
    buffer->size = remained_size;
}

int buffer_read(int fd, buffer_t* buffer);
bool buffer_has_eoh(buffer_t* buffer, int last_buffer_size);

#endif
