#ifndef _JULIA_REQUEST_H_
#define _JULIA_REQUEST_H_

#include "connection.h"


typedef int (*header_processor_t)
        (request_t* request, int offset, response_t* response);

typedef struct {
    int offset;
    header_processor_t processor;
} header_val_t;

void header_map_init(void);
void mime_map_init(void);

void request_init(request_t* request);
void request_clear(request_t* request);
void request_release(request_t* request);

int handle_request(connection_t* connection);


#endif
