#ifndef _JULIA_RESPONSE_H_
#define _JULIA_RESPONSE_H_

#include "connection.h"

#include <stdbool.h>


void response_init(response_t* response);
void response_clear(response_t* response);

void response_build_err(response_t* response, request_t* request, int err);

#endif
