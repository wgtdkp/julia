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

connection_t* open_connection(int fd, pool_t* pool)
{
    connection_t* connection = pool_alloc(pool);
    if (connection == NULL)
        return NULL;

    connection->pool = pool;
    connection->fd = fd;
    set_nonblocking(connection->fd);
    connection->event.events = EVENTS_IN;
    connection->event.data.ptr = connection;
    assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
            connection->fd, &connection->event) != -1);
    
    request_init(&connection->request);
    connection->nrequests = 0;
    
    pool_init(&connection->response_pool, QUEUE_WIDTH(response_t), 16, 1);
    queue_init(&connection->response_queue, &connection->response_pool);   
    
    return connection;
}

void close_connection(connection_t* connection)
{
    close(connection->fd);  // The events automatically removed
    // The order cannot be reversed
    pool_clear(&connection->response_pool);
    queue_clear(&connection->response_queue);
    
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

static int set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}
