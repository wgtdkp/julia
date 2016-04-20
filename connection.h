#ifndef _JULIA_CONNECTION_H_
#define _JULIA_CONNECTION_H_

enum {
    RECV_BUF_SIZE = 4 * 1024,
};

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
    bool keep_alive;
    int content_length;
} Request;

typedef struct {
    int fd; // socket fd
    char buf[RECV_BUF_SIZE];
    int cur;
    Request request;
    Response response;
    // # request during this connection
    int nrequests;
} Connection;

typedef enum {
    RS_OK,
    RS_NOTFOUND,
    RS_DENIED, //resource access denied
} ResourceStat;

#endif
