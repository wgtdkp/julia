#ifndef _JULIA_PARSE_H_
#define _JULIA_PARSE_H_

#include "request.h"

int parse_request_line(request_t* request);
int parse_header_line(request_t* request);
int parse_request_body_chunked(request_t* request);;
int parse_request_body_no_encoding(request_t* request);

#endif
