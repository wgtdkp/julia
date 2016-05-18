#include "connection.h"

#include "request.h"
#include "response.h"

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int epoll_fd;
struct epoll_event events[MAX_EVENT_NUM];
pool_t connection_pool;

static int set_nonblocking(int fd);
static void connection_init(connection_t* connection, int fd, pool_t* pool);

connection_t* open_connection(int fd, pool_t* pool)
{
    connection_t* connection = pool_alloc(pool);
    if (connection == NULL)
        return NULL;

    connection_init(connection, fd, pool);    
    return connection;
}

void close_connection(connection_t* connection)
{
    close(connection->fd);  // The events automatically removed
    pool_free(connection->pool, connection);
}

int add_listener(int* listen_fd)
{
    struct epoll_event ev;
    set_nonblocking(*listen_fd);
    ev.events = EVENTS_IN;
    ev.data.ptr = listen_fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *listen_fd, &ev);
}

int connection_block_request(connection_t* connection)
{
    connection->event.events &= ~EVENTS_IN;
    connection->event.events |= EVENTS_OUT;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
}

int connection_block_response(connection_t* connection)
{
    connection->event.events &= ~EVENTS_OUT;
    connection->event.events |= EVENTS_IN;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
            connection->fd, &connection->event);
}

static void connection_init(connection_t* connection, int fd, pool_t* pool)
{
    connection->pool = pool;
    connection->fd = fd;
    set_nonblocking(connection->fd);
    connection->event.events = EVENTS_IN;
    connection->event.data.ptr = connection;
    assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
            connection->fd, &connection->event) != -1);
    
    request_init(&connection->request);
    response_init(&connection->response);
    connection->nrequests = 0;
}

static int set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}
