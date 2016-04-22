#ifndef _JULIA_CONNECTION_H_
#define _JULIA_CONNECTION_H_

#include "request.h"
#include "response.h"
#include <sys/epoll.h>
#include <stdbool.h>

extern int epoll_fd;
extern struct epoll_event events[MAX_EVENT_NUM];

typedef struct {
    int fd; // socket fd
    Request request;
    Response response;
    int nrequests;  // # request during this connection

    // TODO(wgtdkp): expire time
} Connection;

typedef enum {
    RS_OK,
    RS_NOTFOUND,
    RS_DENIED, //resource access denied
} ResourceStat;

Connection* new_connection(int fd);
void delete_connection(Connection* connection);
void epoll_init(void);
void event_add_listen(int* listen_fd);
void event_add(Connection* connection, int event_flags);
void event_set_to(Connection* connection, int new_event_flags);
void event_delete(Connection* connection, int event_flags);

#endif
