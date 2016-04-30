#include "buffer.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


// Read data to buffer
// Return: if haven't read eof, bytes readed; 
//         else, 0 - readed;
int buffer_read(int fd, buffer_t* buffer)
{
    int readed = 0;
    do {
        int margin = buffer->limit - buffer->end;
        int len = read(fd, buffer->end, margin);
        if (len == 0)   // EOF
            return -readed;
        if (len == -1) {
            if (errno == EWOULDBLOCK)
                break;
            assert(0);
            return ERR_INTERNAL_ERROR;
        }
        readed += len;
        buffer->end += len;
    } while (buffer->end < buffer->limit);  // We may have not read all data
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
    char* end = buffer->data + min(buffer->end - buffer->data - 3, 0);
    // KMP is not really needed here
    for (char* p = end - 1; p >= begin; p--) {
        if (val == *((int*)p))
            return true;
    }

    return false;
}
