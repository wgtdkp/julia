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
union connection_node{
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

// TODO(wgtdkp): lock needed for multithreading
// Return NULL if connections reaches MAX_CONCURRENT_NUM
connection_t* new_connection(int fd)
{
    connection_node_t* connections = connection_pool.connections;
    static bool connections_inited = false;
    if (!connections_inited) {
        connection_pool.cur = &connections[0];
        for (int i = 0; i < MAX_CONCURRENT_NUM - 1; i++) {
            connections[i].next = &connections[i + 1];
        }
        connections[MAX_CONCURRENT_NUM - 1].next = NULL;
        connections_inited = true;
    }

    // cur == NULL means: we are trying to create connections
    // more that MAX_CONCURRENT_NUM
    if (connection_pool.cur == NULL)
        return NULL;
    connection_t* connection = &connection_pool.cur->connection;
    // Set 'cur' to the next node before initialize connection
    connection_pool.cur = connection_pool.cur->next;
    ++connection_pool.allocated;

    // Initialization
    connection->fd = fd;
    set_nonblocking(connection->fd);
    connection->event.events = 0;
    connection->event.data.ptr = connection;
    request_init(&connection->request);
    response_init(&connection->response);
    connection->nrequests = 0;
    
    return connection;
}

// TODO(wgtdkp): lock needed for multithreading
void delete_connection(connection_t* connection)
{
    // Insert the deleted connection back to head
    connection_node_t* tmp = connection_pool.cur;
    connection_pool.cur = (connection_node_t*)connection;
    connection_pool.cur->next = tmp;
    --connection_pool.allocated;
}

void connection_close(connection_t* connection)
{
    close(connection->fd);  // The events automatically removed
    delete_connection(connection);
}

void event_add_listen(int* listen_fd)
{
    struct epoll_event ev;
    set_nonblocking(*listen_fd);
    ev.events = EVENTS_IN;

    ev.data.ptr = listen_fd;
    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *listen_fd, &ev) == -1,
            "epoll_ctl: listen_fd");
}

static int set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}
