#include "server.h"

#define MAX_CONNECTION  (10000)

int epoll_fd;
julia_epoll_event_t events[MAX_EVENT_NUM];
pool_t connection_pool;
pool_t request_pool;
pool_t accept_pool;

static int heap_size = 0;
static connection_t* connections[MAX_CONNECTION + 1] = {NULL};

connection_t* open_connection(int fd) {
    connection_t* c = pool_alloc(&connection_pool);
    c->active_time = time(NULL);
    c->fd = fd;
    c->side = C_SIDE_FRONT;
    c->r = pool_alloc(&request_pool);
    request_init(c->r, c);

    if (connection_register(c) < 0) {
        close_connection(c);
        return NULL;
    }

    set_nonblocking(c->fd);
    c->event.events = EVENTS_IN;
    c->event.data.ptr = c;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, c->fd, &c->event) == -1) {
        close_connection(c);
        return NULL;
    }
    return c;
}

void close_connection(connection_t* c) {
    connection_unregister(c);
    // The events automatically removed
    close(c->fd);
    if (c->side == C_SIDE_FRONT) {
        if (c->r->uc) {
            // We cannot close upstream connection here directly,
            // as there could be unhandled events
            // binded to upstream connection.
            connection_expire(c->r->uc);
        }
        pool_free(&request_pool, c->r);
    } else {
        c->r->uc = NULL;
        c->r->pass = false;
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
    ABORT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    ABORT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
    return 0;
}

#define P(i)    (i / 2)
#define L(i)    (i * 2)
#define R(i)    (L(i) + 1)

static void heap_shift_up(int idx) {
    int k = idx;
    connection_t* c = connections[k];
    while (P(k) > 0) {
        connection_t* pc = connections[P(k)];
        if (c->active_time >= pc->active_time)
            break;
        connections[k] = pc;
        connections[k]->heap_idx = k;
        k = P(k);
    }
    connections[k] = c;
    connections[k]->heap_idx = k;
}

static void heap_shift_down(int idx) {
    int k = idx;
    connection_t* c = connections[k];
    while (true) {
        int kid = L(k);
        if (R(k) <= heap_size &&
            connections[R(k)]->active_time < connections[L(k)]->active_time) {
            kid = R(k);
        }
        if (kid > heap_size ||
            c->active_time < connections[kid]->active_time) {
            break;
        }
        connections[k] = connections[kid]; 
        connections[k]->heap_idx = k;
        k = kid;
    }
    connections[k] = c;
    connections[k]->heap_idx = k;    
}

void connection_active(connection_t* c) {
    c->active_time = time(NULL);
    heap_shift_down(c->heap_idx);
}

void connection_expire(connection_t* c) {
    c->active_time = time(NULL) - server_cfg.timeout - 1;
    heap_shift_up(c->heap_idx);
}

// Return: 0, success; -1, fail;
int connection_register(connection_t* c) {
    if (heap_size + 1 > MAX_CONNECTION)
      return -1;
    connections[++heap_size] = c;
    heap_shift_up(heap_size);
    return 0;
}

void connection_unregister(connection_t* c) {
    assert(heap_size > 0);
    connections[c->heap_idx] = connections[heap_size];
    connections[c->heap_idx]->heap_idx = c->heap_idx;
    --heap_size;
    if (heap_size > 0) {
        heap_shift_down(c->heap_idx);
    }
}

void connection_sweep(void) {
    while (heap_size > 0) {
        connection_t* c = connections[1];
        if (time(NULL) >= c->active_time + server_cfg.timeout) {
            close_connection(c);
        } else {
            break;
        }
    }
}
