#include "connection.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

typedef union ConnectionNode ConnectionNode;
union ConnectionNode{
    Connection connection;
    ConnectionNode* next;
};

// TODO(wgtdkp): make max concurrent connection configurable ?
struct {
    ConnectionNode* cur;
    ConnectionNode connections[MAX_CONCURRENT_NUM];
    int allocated;
} connection_pool = {
    .cur = NULL,
    .allocated = 0,
};

int epoll_fd;
struct epoll_event events[MAX_EVENT_NUM];

static void set_nonblocking(int fd);

// TODO(wgtdkp): lock needed for multithreading
// Return NULL if connections reaches MAX_CONCURRENT_NUM
Connection* new_connection(int fd)
{
    ConnectionNode* connections = connection_pool.connections;
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
    Connection* connection = &connection_pool.cur->connection;
    // Set 'cur' to the next node before initialize connection
    connection_pool.cur = connection_pool.cur->next;
    ++connection_pool.allocated;

    // Initialization
    connection->fd = fd;
    request_init(&connection->request);
    response_init(&connection->response);
    connection->nrequests = 0;
    
    return connection;
}

// TODO(wgtdkp): lock needed for multithreading
void delete_connection(Connection* connection)
{
    // TODO(wgtdkp): release resource
    request_clear(&connection->request);
    response_clear(&connection->response);
    connection->nrequests = 0;

    // insert the deleted connection back to head
    ConnectionNode* tmp = connection_pool.cur;
    connection_pool.cur = (ConnectionNode*)connection;
    connection_pool.cur->next = tmp;
    --connection_pool.allocated;
    // Closing fd will automatically delete events bind to it
    //close(connection->fd);
    //event_delete(connection, EVENTS_IN);
    //event_delete(connection, EVETNS_OUT);
}

void epoll_init(void)
{
    epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == -1, "epoll_createl");
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

void event_add(Connection* connection, int event_flags)
{
    struct epoll_event ev;
    set_nonblocking(connection->fd);
    ev.events = event_flags;
    
    ev.data.ptr = connection;
    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connection->fd, &ev) == -1,
            "epoll_ctl: connection_fd");
}

void event_set_to(Connection* connection, int new_event_flags)
{
    struct epoll_event ev;
    set_nonblocking(connection->fd);
    ev.events = new_event_flags;

    ev.data.ptr = connection;
    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connection->fd, &ev) == -1,
            "epoll_ctl: connection_fd");
}

void event_delete(Connection* connection, int event_flags)
{
    struct epoll_event ev;
    ev.events = event_flags;

    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection->fd, &ev) == -1,
            "epoll_ctl: event_fd");
}

static void set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
}
