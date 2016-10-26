#include "server.h"

int epoll_fd;
julia_epoll_event_t events[MAX_EVENT_NUM];
pool_t connection_pool;
pool_t request_pool;
pool_t accept_pool;

connection_t* open_connection(int fd) {
    connection_t* c = pool_alloc(&connection_pool);

    c->fd = fd;
    c->side = C_SIDE_FRONT;
    
    set_nonblocking(c->fd);
    c->event.events = EVENTS_IN;
    c->event.data.ptr = c;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, c->fd, &c->event) == -1) {
        close_connection(c);
        return NULL;
    }
    //bzero(&c->event, sizeof(c->event));
    //c->event.data.ptr = c;
    //connection_enable_in(c);    
    
    c->r = pool_alloc(&request_pool);
    request_init(c->r, c);
    //c->nrequests = 0;
    return c;
}

void close_connection(connection_t* c) {
    // The events automatically removed
    close(c->fd);
    if (c->side == C_SIDE_FRONT) {
        if (c->r->uc) {
            close_connection(c->r->uc);
            c->r->uc = NULL;
        }
        pool_free(&request_pool, c->r);
    } else {
        c->r->uc = NULL;
    }
    pool_free(&connection_pool, c);
}

int add_listener(int* listen_fd) {
    julia_epoll_event_t ev;
    set_nonblocking(*listen_fd);
    ev.events = EVENTS_IN;
    ev.data.ptr = listen_fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *listen_fd, &ev);
}

int set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}
