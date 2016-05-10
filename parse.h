#ifndef _JULIA_PARSE_H_
#define _JULIA_PARSE_H_

#include "request.h"

int parse_request_line(request_t* request);
int parse_header_line(request_t* request);
int parse_request_body(request_t* request);;

#endif
