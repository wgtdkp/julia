#ifndef _JULIA_RESPONSE_H_
#define _JULIA_RESPONSE_H_

#include "server.h"
#include <stdbool.h>

typedef struct {
    int status;
    const char* content_type;
    int content_fd;
    int is_script;

    int send_cur;
    char send_buf[SEND_BUF_SIZE];
    bool ready; // response has been fully constructed
} Response;

void response_init(Response* response);
void response_clear(Response* response);

#endif
