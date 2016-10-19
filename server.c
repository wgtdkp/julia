#include "server.h"

#include "base/map.h"

#include "config.h"
#include "connection.h"
#include "parse.h"
#include "request.h"
#include "response.h"
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


#define DEBUG(msg)  fprintf(stderr, "%s\n", (msg));

int doc_root_fd;

// DEBUG:
clock_t total = 0;
int total_reqs = 0;
int max_connection = 0;
int max_response = 0;

static int startup(unsigned short port);
static int server_init(char* cfg_file);
static void usage(void);

static void sig_int(int signo)
{
    float total_time = 1.0f * total / CLOCKS_PER_SEC;
    printf("[%d]: time: %f, total reqs: %d, RPS: %f\n",
            getpid(), total_time, total_reqs, total_reqs / total_time);
    printf("connection pool allocated: %d\n", connection_pool.nallocated);
    printf("max connection allocated: %d\n", max_connection);
    printf("response pool allocated: %d\n", response_pool.nallocated);
    printf("max response allocated: %d\n", max_response);
    exit(0);
}

static int startup(uint16_t port)
{
    // If the client closed the connection, then it will cause SIGPIPE
    // Here simplely ignore this SIG
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_int);
    
    int listen_fd = 0;
    struct sockaddr_in server_addr = {0};
    int addr_len = sizeof(server_addr);
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        return -1;
    }

    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef REUSE_PORT
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
    


    memset((void*)&server_addr, 0, addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr*)&server_addr, addr_len) < 0) {
        return -1;
    }

    // TODO(wgtdkp): make parameter 'backlog' configurable?
    if (listen(listen_fd, 1024) < 0) {
        return -1;
    }
    return listen_fd;
}

static int server_init(char* cfg_file)
{
    // Set limits
    struct rlimit nofile_limit = {65535, 65535};
    setrlimit(RLIMIT_NOFILE, &nofile_limit);
    
    // Config init
    if (config_load(&server_cfg, cfg_file) != OK)
        return ERROR;
    
    parse_init();
    header_map_init();
    mime_map_init();
    
    pool_init(&connection_pool, sizeof(connection_t), 8, 0);
    pool_init(&response_pool, QUEUE_WIDTH(response_t), 8, 0);
    //pool_init(&accept_pool, LIST_WIDTH(accept_type_t), 4, 0);
    
    
    epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == -1, "epoll_create1");

    doc_root_fd = open(server_cfg.doc_root.data, O_RDONLY);
    EXIT_ON(doc_root_fd == -1, "open(doc_root)");
    
    return OK;
}

static void usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    julia config_file\n");
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        usage();
        exit(-1);
    }

#ifdef REUSE_PORT
#define NCORES  (8)
    int workers[NCORES];
    for (int i = 0; i < NCORES - 1; i++) {
        int pid = fork();
        if (pid == 0) {
            goto work;
        } else {
            workers[i] = pid;
        }
    }
    workers[NCORES - 1] = getpid();
    cpu_set_t mask;
    for (int i = 0; i < NCORES; i++) {
        CPU_ZERO(&mask);
        CPU_SET(i, &mask);
        sched_setaffinity(workers[i], sizeof(cpu_set_t), &mask);
        //sched_getaffinity(NCORES[i], sizeof(cpu_set_t), &mask);
        
    }

work:
#endif    
    int listen_fd = -1;

    if (server_init(argv[1]) != OK)
        return -1;
    
    listen_fd = startup(server_cfg.port);
    if (listen_fd < 0) {
        fprintf(stderr, "startup server failed\n");
        exit(-1);
    }

    printf("julia started...\n");
    printf("listening at port: %u\n", server_cfg.port);
    print_string("doc root: %*s\n", &server_cfg.doc_root);
    fflush(stdout);

    assert(add_listener(&listen_fd) != -1);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, 3000);
        if (nfds == -1) {
            EXIT_ON(errno != EINTR, "epoll_wait");
            continue;
        }
        
        clock_t begin = clock();
        //TODO(wgtdkp): multithreading here: seperate fds to several threads
        for (int i = 0; i < nfds; i++) {
            // Here is a hacking: 
            // Eeven if events[i].data is set to pointer of connection,
            // we can get it's fd correctly(as 'fd' is the first
            // member of struct connection_t).
            int fd = *((int*)(events[i].data.ptr));
            if (fd == listen_fd) {
                // We could accept more than one connection per request
                while (true) {
                    int connection_fd = accept(fd, NULL, NULL);
                    if (connection_fd == -1) {
                        // TODO(wgtdkp): handle this error
                        // There could be too many connections(beyond OPEN_MAX)
                        EXIT_ON((errno != EWOULDBLOCK), "accept");
                        break;
                    }
                    connection_t* connection =
                            open_connection(connection_fd, &connection_pool);
                    if (connection == NULL) {
                        close(connection_fd);
                        fprintf(stderr, "too many concurrent connection\n");
                    }
                    max_connection = max(max_connection, connection_pool.nallocated);
                }
                continue;
            }
            if (events[i].events & EPOLLIN) {
                // Receive request or data
                ++total_reqs;
                max_response = max(max_response, response_pool.nallocated);
                connection_t* connection = (connection_t*)(events[i].data.ptr);
                handle_request(connection);
            }
            // TODO(wgtdkp): checking errors?
            if (events[i].events & EPOLLOUT) {
                // Send response
                connection_t* connection = (connection_t*)(events[i].data.ptr);
                handle_response(connection);
            }
        }
        total += clock() - begin;
    }

    close(listen_fd);
    return 0;
}
