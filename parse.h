#ifndef _JULIA_PARSE_H_
#define _JULIA_PARSE_H_

#include "request.h"

int parse_request_line(request_t* request);
int request_parse(request_t* request);

#endif
