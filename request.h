#ifndef _JULIA_REQUEST_H_
#define _JULIA_REQUEST_H_

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

typedef struct {
    Method method;
    int version[2];
    String query_string;
    String path;
    
    /* header entities */
    bool keep_alive;
    int content_length;

    int recv_cur;
    char recv_buf[RECV_BUF_SIZE];
    bool ready; // request has been fully constructed
} Request;

void request_init(Request* request);
void request_release(Request* request);
void request_clear(Request* request);
int request_parse(Request* request);

#endif
