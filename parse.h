#ifndef _JULIA_PARSE_H_
#define _JULIA_PARSE_H_

#include "request.h"


void parse_init(void);
int parse_request_line(request_t* request);
int parse_header_line(request_t* request);
int parse_request_body_chunked(request_t* request);;
int parse_request_body_identity(request_t* request);
int parse_accept(request_t* request);


#endif
