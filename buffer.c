#include "buffer.h"


// TODO(wgtdkp): move all such short functions into headers
void buffer_init(Buffer* buffer)
{
    buffer->size = 0;
    buffer->capacity = RECV_BUF_SIZE;
}

