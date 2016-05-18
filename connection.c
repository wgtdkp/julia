#include "connection.h"

#include "request.h"
#include "response.h"

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef union connection_node connection_node_t;
union connection_node {
    connection_t connection;
    connection_node_t* next;
};

// TODO(wgtdkp): make max concurrent connection configurable ?
struct {
    connection_node_t* cur;
    connection_node_t connections[MAX_CONCURRENT_NUM];
    int allocated;
} connection_pool = {
    .cur = NULL,
    .allocated = 0,
};

int epoll_fd;
struct epoll_event events[MAX_EVENT_NUM];

static int set_nonblocking(int fd);
static void delete_connection(connection_t* connection);
static void connection_init(connection_t* connection, int fd);


void connection_pool_init(void)
{
    connection_node_t* connections = connection_pool.connections;
    connection_pool.cur = &connections[0];
    for (int i = 0; i < MAX_CONCURRENT_NUM - 1; i++) {
        connections[i].next = &connections[i + 1];
    }
    connections[MAX_CONCURRENT_NUM - 1].next = NULL;
}

// TODO(wgtdkp): lock needed for multithreading
// Return NULL if connections reaches MAX_CONCURRENT_NUM
connection_t* new_connection(int fd)
{
    // cur == NULL means: we are trying to create connections
    // more that MAX_CONCURRENT_NUM
    if (connection_pool.cur == NULL)
        return NULL;

    connection_t* connection = &connection_pool.cur->connection;
    // Set 'cur' to the next node before initialize connection
    connection_pool.cur = connection_pool.cur->next;
    ++connection_pool.allocated;

    connection_init(connection, fd);
    
    return connection;
}

void connection_close(connection_t* connection)
{
    close(connection->fd);  // The events automatically removed
    delete_connection(connection);
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

static void connection_init(connection_t* connection, int fd)
{
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

// TODO(wgtdkp): lock needed for multithreading
static void delete_connection(connection_t* connection)
{
    // Insert the deleted connection back to allocation cursor
    connection_node_t* tmp = connection_pool.cur;
    connection_pool.cur = (connection_node_t*)connection;
    connection_pool.cur->next = tmp;
    --connection_pool.allocated;
}

static int set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}
