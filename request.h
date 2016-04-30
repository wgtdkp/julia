#ifndef _JULIA_REQUEST_H_
#define _JULIA_REQUEST_H_

#include "connection.h"

void request_init(request_t* request);
void request_release(request_t* request);
void request_clear(request_t* request);

int handle_request(connection_t* connection);

#endif
