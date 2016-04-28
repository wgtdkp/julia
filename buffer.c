#include "buffer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Read data to buffer
// Return: bytes readed; 0, indicates end of file;
int buffer_read(int fd, buffer_t* buffer)
{
    int readed = 0;
    do {
        int margin = buffer->capacity - buffer->size;
        int len = read(fd, buffer->data + buffer->size, margin);
        if (len == 0)   // EOF
            return 0;
        if (len == -1) {
            if (errno == EWOULDBLOCK)
                break;
            EXIT_ON(true, "read");  // Unhandled error
        }
        readed += len;
        buffer->size += len;
    } while (buffer->size < buffer->capacity);  // We may have not read all data
    return readed;
}

// Detect the end of (request) header
bool buffer_has_eoh(buffer_t* buffer, int last_buffer_size)
{
    // TODO(wgtdkp): what about waiting for body?
    const char* end_of_header = "\r\n\r\n";
    const int val = *((int*)end_of_header);
    // We restart detecting end of header from the last position we stoped
    char* begin = buffer->data + max(last_buffer_size - 3, 0);
    char* end = buffer->data + min(buffer->size - 4, 0);
    // KMP is not really needed here
    for (char* p = begin; p < end; p++) {
        if (val == *((int*)p))
            return true;
    }

    return false;
}
