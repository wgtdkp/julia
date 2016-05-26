#include "buffer.h"
#include "string.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>


// Read data to buffer
// Return: if haven't read eof, bytes readed; 
//         else, 0 - readed;
int buffer_recv(buffer_t* buffer, int fd)
{
    int readed = 0;
    do {
        int margin = buffer->limit - buffer->end;
        int len = recv(fd, buffer->end, margin, 0);
        if (len == 0)   // EOF
            return -readed;
        if (len == -1) {
            if (errno == EAGAIN)
                break;
            EXIT_ON(1, "recv");
            return ERR_INTERNAL_ERROR;
        }
        readed += len;
        buffer->end += len;
    } while (buffer->end < buffer->limit);  // We may have not read all data
    return readed;
}

int buffer_send(buffer_t* buffer, int fd)
{
    int sent = 0;
    do {
        int len = send(fd, buffer->begin, buffer_size(buffer), 0);
        if (len == -1) {
            if (errno == EAGAIN)
                break;
            else if (errno == EPIPE) {
                // TODO(wgtdkp): the connection is broken
            }
            EXIT_ON(1, "send");
            return ERR_INTERNAL_ERROR;
        }
        sent += len;
        buffer->begin += len;
    } while (buffer_size(buffer) != 0);
    return sent;
}

int buffer_append_string(buffer_t* buffer, const string_t str)
{
    int margin = buffer->limit - buffer->end;
    int appended = min(margin, str.len);
    memcpy(buffer->end, str.data, appended);
    buffer->end += appended;
    return appended;
}

int buffer_print(buffer_t* buffer, const char* format, ...)
{
    va_list args;
    va_start (args, format);
    int margin = buffer->limit - buffer->end;
    int len = vsnprintf(buffer->end, margin, format, args);
    buffer->end += len;
    va_end (args);
    return len;
}

