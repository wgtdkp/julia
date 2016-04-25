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
} RequestStatus;

typedef struct {
    Method method;
    int version[2];
    String query_string;
    String path;

    /* header entities */
    bool keep_alive;
    int content_length;

    Buffer buffer;
    RequestStatus status;
} Request;

void request_init(Request* request);
void request_release(Request* request);
void request_clear(Request* request);
int request_parse(Request* request);

#endif
