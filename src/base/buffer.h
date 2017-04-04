#ifndef _JULIA_BUFFER_H_
#define _JULIA_BUFFER_H_

#include "string.h"
#include "util.h"

#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

#define BUF_SIZE   (2 * 1024)

typedef struct {
    char* begin; // inclusive
    char* end; // exclusive
    char data[BUF_SIZE + 1];
    char limit[1];
} buffer_t;

static inline void buffer_init(buffer_t* buffer) {
    buffer->begin = buffer->data;
    buffer->end = buffer->data;
}

static inline void buffer_clear(buffer_t* buffer) {
    buffer_init(buffer);
}

static inline int buffer_size(buffer_t* buffer) {
    return buffer->end - buffer->begin;
}

static inline int buffer_append_u8(buffer_t* buffer, uint8_t val) {
    if (buffer->limit <= buffer->end)
        return 0;
    *buffer->end++ = val;
    return 1;
}

static inline int buffer_append_u16le(buffer_t* buffer, uint16_t val) {
    int len = buffer_append_u8(buffer, val & 0xff);
    return len + buffer_append_u8(buffer, (val >> 8) & 0xff);
}

static inline int buffer_append_u32le(buffer_t* buffer, uint32_t val) {
    int len = buffer_append_u16le(buffer, val & 0xffff);
    return len + buffer_append_u16le(buffer, (val >> 16) & 0xffff);
}

// Discard the received data that has been successfully parsed.
// The remaining data is copied to the head of the buffer for next parsing.
static inline void buffer_discard_parsed(buffer_t* buffer) {
    int size = buffer_size(buffer);
    // TODO(wgtdkp): Is it safe?
    // There shouldn't be much data remained
    memcpy(buffer->data, buffer->begin, size);
    buffer->begin = buffer->data;
    buffer->end = buffer->begin + size;
}

static inline bool buffer_full(buffer_t* buffer) {
    return buffer->end >= buffer->limit;
}

static inline int buffer_margin(buffer_t* buffer) {
    return buffer->limit - buffer->end;
}

static inline void buffer_append(buffer_t* des, buffer_t* src) {
    assert(buffer_margin(des) >= buffer_size(src));
    memcpy(des->end, src->begin, buffer_size(src));
    des->end += buffer_size(src);
}

int buffer_recv(buffer_t* buffer, int fd);
int buffer_send(buffer_t* buffer, int fd);
int buffer_append_string(buffer_t* buffer, const string_t* str);
int buffer_print(buffer_t* buffer, const char* format, ...);
void print_buffer(buffer_t* buffer);

#define buffer_append_cstring(buffer, cstr) buffer_append_string(buffer, &STRING(cstr))

#endif
