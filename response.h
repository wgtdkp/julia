#ifndef _JULIA_RESPONSE_H_
#define _JULIA_RESPONSE_H_

#include "connection.h"

#include <stdbool.h>

void mime_map_init(void);

void response_init(response_t* response);
void response_clear(response_t* response);

int handle_response(connection_t* connection);
int response_build(response_t* response, request_t* request);
void response_build_err(response_t* response, request_t* request, int err);

#endif
