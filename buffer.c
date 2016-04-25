#include "buffer.h"


// TODO(wgtdkp): move all such short functions into headers
void buffer_init(buffer_t* buffer)
{
    buffer->size = 0;
    buffer->capacity = RECV_BUF_SIZE;
}

