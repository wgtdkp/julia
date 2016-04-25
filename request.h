#ifndef _JULIA_REQUEST_H_
#define _JULIA_REQUEST_H_

#include "buffer.h"
#include "server.h"
#include "string.h"
#include <stdbool.h>

typedef enum {
    M_GET,
    M_POST,
    M_UNIMP,    //below is not implemented
    M_PUT,
    M_DELETE,
    M_TRACE,
    M_CONNECT,
    M_HEAD,
    M_OPTIONS,
} Method;

typedef enum {
    RS_WF_LINE,     // Waiting for request line
    RS_WF_HEADER,   // Waiting for request header
    RS_WF_BODY,     // Waiting for request body
    RS_READY,       // request is ready
} request_tStatus;

typedef struct {
    Method method;
    int version[2];
    string_t query_string;
    string_t path;

    /* header entities */
    bool keep_alive;
    int content_length;

    buffer_t buffer;
    request_tStatus status;
} request_t;

void request_init(request_t* request);
void request_release(request_t* request);
void request_clear(request_t* request);
int request_parse(request_t* request);

#endif
