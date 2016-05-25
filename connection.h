#ifndef _JULIA_CONNECTION_H_
#define _JULIA_CONNECTION_H_

#include "base/buffer.h"
#include "base/pool.h"
#include "base/queue.h"
#include "base/string.h"

#include "server.h"

#include <stdbool.h>

#include <sys/epoll.h>
#include <sys/stat.h>


extern int epoll_fd;
extern struct epoll_event events[MAX_EVENT_NUM];
extern pool_t connection_pool;
extern pool_t response_pool;
extern pool_t accept_pool;


#define COMMON_HEADERS              \
    /* General headers */           \
    string_t cache_control;         \
    string_t connection;            \
    string_t date;                  \
    string_t pragma;                \
    string_t trailer;               \
    string_t transfer_encoding;     \
    string_t upgrade;               \
    string_t via;                   \
    string_t warning;               \
    /* Entity headers */            \
    string_t allow;                 \
    string_t content_encoding;      \
    string_t content_language;      \
    string_t content_length;        \
    string_t content_location;      \
    string_t content_md5;           \
    string_t content_range;         \
    string_t content_type;          \
    string_t expires;               \
    string_t last_modified;

typedef struct {
    COMMON_HEADERS
    string_t accept;
    string_t accept_charset;
    string_t accept_encoding;
    string_t authorization;
    string_t expect;
    string_t from;
    string_t host;
    string_t if_match;
    string_t if_modified_since;
    string_t if_none_match;
    string_t if_range;
    string_t if_unmodified_since;
    string_t max_forwards;
    string_t proxy_authorization;
    string_t range;
    string_t referer;
    string_t te;
    string_t user_agent;
} request_headers_t;

typedef struct {
    COMMON_HEADERS
    string_t accept_ranges;
    string_t age;
    string_t etag;
    string_t location;
    string_t proxy_authenticate;
    string_t retry_after;
    string_t server;
    string_t vary;
    string_t www_authenticate;
} response_headers_t;



typedef struct {
    string_t scheme;
    string_t host;
    string_t port;
    string_t abs_path;
    string_t extension;
    string_t query;
    int nddots;
    int nentries;
} uri_t;

/*
 * Request
 */

typedef enum {
    M_CONNECT,
    M_DELETE,
    M_GET,
    M_HEAD,
    M_OPTIONS,
    M_POST,
    M_PUT,
    M_TRACE,
} method_t;

typedef enum {
    RS_REQUEST_LINE,
    RS_HEADERS,
    RS_BODY,
} request_stage_t;

// Tranfer coding
typedef enum {
    TE_IDENTITY,
    TE_CHUNKED,
    TE_GZIP,
    TE_COMPRESS,
    TE_DEFLATE,
} transfer_encoding_t;

typedef struct {
    string_t type;
    string_t subtype;
    float q;
} accept_type_t;

typedef struct {
    method_t method;
    struct {
        unsigned short major;
        unsigned short minor;
    } version;
    request_headers_t headers;
    //int status;
    list_t accepts;

    // For state machine
    int state;
    string_t request_line;
    string_t header_name;
    string_t header_value;
    int uri_state;
    uri_t uri;

    request_stage_t stage;
    bool keep_alive;
    transfer_encoding_t t_encoding;
    int content_length;

    buffer_t buffer;
} request_t;

/*
 * Response
 */

typedef struct {
    int resource_fd;
    struct stat resource_stat;
    int status;
    response_headers_t headers;
    // Connection must be closed after the response was sent.
    // This happens when we accept a bad syntax request, and
    // cannot recover from this status. Because we immediately
    // discard the request and we thus cannot decide the end
    // of the bad request or the beginning of next request.
    bool must_close;
    //response_headers_t headers;
    buffer_t buffer;

} response_t;

/*
typedef struct {
    response_t* front;
    response_t* back;  
} response_queue_t;

typedef struct {
    
    response_t* cur;
    int allocated; 
};
*/

/*
 * Connection
 */

 typedef struct {
    int fd; // socket fd
    struct epoll_event event;

    request_t request;
    queue_t response_queue;
    //response_t response;
    int nrequests;  // # request during this connection

    // TODO(wgtdkp): expire time

    pool_t* pool;
} connection_t;

connection_t* open_connection(int fd, pool_t* pool);
void close_connection(connection_t* connection);
int add_listener(int* listen_fd);


static inline int connection_disable_in(connection_t* connection)
{
    connection->event.events &= ~EVENTS_IN;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
}

static inline int connection_enable_in(connection_t* connection)
{
    connection->event.events |= EVENTS_IN;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
}

static inline int connection_disable_out(connection_t* connection)
{
    connection->event.events &= ~EVENTS_OUT;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
}

static inline int connection_enable_out(connection_t* connection)
{
    connection->event.events |= EVENTS_OUT;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
} 

#endif
