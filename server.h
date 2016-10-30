#ifndef _JULIA_SERVER_H_
#define _JULIA_SERVER_H_

#include "base/buffer.h"
#include "base/map.h"
#include "base/pool.h"
#include "base/queue.h"
#include "base/string.h"
#include "base/vector.h"
#include "event.h"
#include "server.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/sendfile.h>
#include <sys/un.h>

#define MAX_EVENT_NUM   (65536)
#define MAX_CONNECTION  (10000)

#define EVENTS_IN   (EPOLLIN)
#define EVENTS_OUT  (EPOLLOUT)

/*
 * Config
 */
typedef enum {
    PROT_HTTP,
    PROT_UWSGI, // support only uwsgi
    PROT_FCGI,
} protocol_t;

typedef struct {
    bool pass;
    int fd;
    string_t path;
    string_t root;
    string_t host;
    uint16_t port;
    protocol_t protocol;
} location_t;

typedef struct {
    uint16_t port;
    int root_fd;
    bool debug;
    bool daemon;
    int timeout;
    vector_t workers;
    vector_t locations;
    char* text;
} config_t;

extern config_t server_cfg;

int config_load(config_t* cfg);
void config_destroy(config_t* cfg);

/*
 * Connection
 */
extern int epoll_fd;
extern julia_epoll_event_t events[MAX_EVENT_NUM];
extern pool_t connection_pool;
extern pool_t request_pool;
extern pool_t accept_pool;


#define COMMON_HEADERS              \
    /* General headers */           \
    string_t cache_control;         \
    string_t c;            \
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
    string_t cookie;
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

/*
 * Response
 */
/*
typedef struct {
    int status;
    int resource_fd;
    int body_fd;
    int body_len;
    bool fetch_from_back;
    struct back_connection* back_connection;
    struct stat resource_stat;
    response_headers_t headers;
    // Connection must be closed after the response was sent.
    // This happens when we accept a bad syntax request, and
    // cannot recover from this status. Because we immediately
    // discard the request and we thus cannot decide the end
    // of the bad request or the beginning of next request.
    bool keep_alive;
    
    //response_headers_t headers;
    buffer_t buffer;
} response_t;
*/

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
    RS_PASS_HEADERS,
    RS_PASS_BODY,
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
    uint16_t major;
    uint16_t minor;
} version_t;

typedef struct {
    string_t scheme;
    string_t host;
    string_t port;
    string_t abs_path;
    string_t extension;
    string_t query;
    int nddots;
    int nentries;
    int state;
} uri_t;


typedef struct request {
    method_t method;
    version_t version;
    request_headers_t headers;
    list_t accepts;

    // For state machine
    int state;
    string_t request_line;
    string_t header_name;
    string_t header_value;
    uri_t uri;
    string_t host;
    uint16_t port;

    request_stage_t stage;
    
    uint8_t discard_body: 1;
    uint8_t body_done: 1;
    uint8_t done: 1;
    uint8_t response_done: 1;
    uint8_t keep_alive: 1;

    transfer_encoding_t t_encoding;
    int content_length;
    int body_received;
    
    bool pass;
    buffer_t rb;
    buffer_t sb;
    struct connection* c;
    struct connection* uc;
    //typedef int (*request_handler_t)(struct request* r);
    int (*in_handler)(struct request* r);
    int (*out_handler)(struct request* r);
    //request_handler_t* pass_handler;
    //request_handler_t* fetch_handler;

    int status;
    int resource_fd;
    int resource_len;
} request_t;

/*
 * Connection
 */
enum {
    C_SIDE_FRONT,
    C_SIDE_BACK,
};

typedef struct connection {
    int fd;
    int side;
    julia_epoll_event_t event;
    request_t* r;
    time_t active_time;
    int heap_idx;
} connection_t;

#define HTTP_1_1    (version_t){1, 1}
#define HTTP_1_0    (version_t){1, 0}

connection_t* open_connection(int fd);
void close_connection(connection_t* c);
int add_listener(int* listen_fd);
int set_nonblocking(int fd);
int connection_comp(void* lhs, void* rhs);
void connection_active(connection_t* c);
void connection_register(connection_t* c);
void connection_unregister(connection_t* c);
void connection_sweep(void);

static inline int connection_disable_in(connection_t* c) {
    if (c->event.events & EVENTS_IN) {
        c->event.events &= ~EVENTS_IN;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                         c->fd, &c->event);
    }
    return 0;
}

static inline int connection_enable_in(connection_t* c) {
    if (!(c->event.events & EVENTS_IN)) {
        c->event.events |= EVENTS_IN;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                         c->fd, &c->event);
    }
    return 0;
}

static inline int connection_disable_out(connection_t* c) {
    if (c->event.events & EVENTS_OUT) {
        c->event.events &= ~EVENTS_OUT;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                         c->fd, &c->event);
    }
    return 0;
}

static inline int connection_enable_out(connection_t* c) {
    if (!(c->event.events & EVENTS_OUT)) {
        c->event.events |= EVENTS_OUT;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                         c->fd, &c->event);
    }
    return 0;
} 

/*
 * Request
 */
typedef int (*header_processor_t)(request_t* request, int offset);
void header_map_init(void);
void request_init(request_t* r, connection_t* c);
void request_clear(request_t* request);
void request_release(request_t* request);
int handle_request(connection_t* c);
int handle_response(connection_t* c);
int handle_pass(connection_t* uc);
int handle_upstream(connection_t* uc);
int send_response_buffer(request_t* r);
int send_response_file(request_t* r);
/*
 * Response
 */
void mime_map_init(void);

//int handle_response(connection_t* c);
int response_build(request_t* r);
int response_build_err(request_t* request, int err);


/*
 * Parse
 */
// State machine: request line states
enum {
    // Request line states
    RL_S_BEGIN = 0,
    RL_S_METHOD,
    RL_S_SP_BEFORE_URI,
    RL_S_URI,
    RL_S_SP_BEFROE_VERSION,
    RL_S_HTTP_H,
    RL_S_HTTP_HT,
    RL_S_HTTP_HTT,
    RL_S_HTTP_HTTP,
    RL_S_HTTP_VERSION_SLASH,
    RL_S_HTTP_VERSION_MAJOR,
    RL_S_HTTP_VERSION_DOT,
    RL_S_HTTP_VERSION_MINOR,
    RL_S_SP_AFTER_VERSION,
    RL_S_ALMOST_DONE,
    RL_S_DONE,

    // Header line states
    HL_S_BEGIN,
    HL_S_IGNORE,
    HL_S_NAME,
    HL_S_COLON,
    HL_S_SP_BEFORE_VALUE,
    HL_S_VALUE,
    HL_S_SP_AFTER_VALUE,
    HL_S_ALMOST_DONE,
    HL_S_DONE,

    // URI states
    URI_S_BEGIN,
    URI_S_SCHEME,
    URI_S_SCHEME_COLON,
    URI_S_SCHEME_SLASH,
    URI_S_SCHEME_SLASH_SLASH,
    URI_S_HOST,
    URI_S_PORT,
    URI_S_ABS_PATH_DOT,
    URI_S_ABS_PATH_DDOT,
    URI_S_ABS_PATH_SLASH,
    URI_S_ABS_PATH_ENTRY,
    URI_S_EXTENSION,
    URI_S_QUERY,
};


void parse_init(void);
int parse_request_line(request_t* request);
int parse_header_line(request_t* request);
int parse_request_body_chunked(request_t* request);
int parse_request_body_identity(request_t* request);
int parse_header_accept(request_t* request);
void parse_header_host(request_t* request);

/*
 * uWSGI
 */
//int uwsgi_start_request(connection_t* c);
int uwsgi_open_connection(request_t* r, location_t* loc);

#endif
