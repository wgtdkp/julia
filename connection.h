#ifndef _JULIA_CONNECTION_H_
#define _JULIA_CONNECTION_H_

#include "buffer.h"
#include "server.h"
#include "string.h"

#include <sys/epoll.h>
#include <stdbool.h>

extern int epoll_fd;
extern struct epoll_event events[MAX_EVENT_NUM];

/********************
 * Request
 ********************/

typedef enum {
    M_CONNECT = 0,
    M_DELETE,
    M_GET,
    M_HEAD,
    M_OPTIONS,
    M_POST,
    M_PUT,
    M_TRACE,
} method_t;

typedef struct {
    method_t method;
    struct {
        int major;
        int minor;
    } version;

    int status;

    // For state machine
    string_t request_line;
    string_t method_unparsed;
    string_t uri;
    string_t schema;
    string_t host;
    string_t header_name;
    string_t header_value;

    int state;
    int uri_state;
    bool keep_alive;
    bool invalid_header;
    bool request_line_done;
    bool headers_done;
    bool body_done;
    bool saw_eof;
    buffer_t buffer;
} request_t;


/*********************
 * Response
 *********************/

typedef struct {
    int status;
    const char* content_type;
    int content_fd;
    int is_script;

    int send_cur;
    char send_buf[SEND_BUF_SIZE];
    bool ready; // response has been fully constructed
} response_t;


/*******************
 * Connection
 *******************/

 typedef struct {
    int fd; // socket fd
    request_t request;
    response_t response;
    int nrequests;  // # request during this connection

    // TODO(wgtdkp): expire time
} connection_t;


connection_t* new_connection(int fd);
void delete_connection(connection_t* connection);
void connection_close(connection_t* connection);

void epoll_init(void);
void event_add_listen(int* listen_fd);
void event_add(connection_t* connection, int event_flags);
void event_set_to(connection_t* connection, int new_event_flags);
void event_delete(connection_t* connection, int event_flags);

#endif
