#include "server.h"

#include "base/map.h"

#include "connection.h"
#include "request.h"
#include "response.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
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

static int startup(unsigned short port);
static int server_init(const char* doc_root);
static void usage(void);

static void sig_int(int signo)
{
    // TODO(wgtdkp): print statistic info.
    
}

static int startup(unsigned short port)
{
    // If the client closed the connection, then it will cause SIGPIPE
    // Here simplely ignore this SIG
    signal(SIGPIPE, SIG_IGN);
    //signal(SIGINT, sig_int);
    
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

static int server_init(const char* doc_root)
{
    // Set limits
    struct rlimit nofile_limit = {65535, 65535};
    setrlimit(RLIMIT_NOFILE, &nofile_limit);
    
    
    header_map_init();
    mime_map_init();
    // Init pool with 256 connection
    pool_init(&connection_pool, sizeof(connection_t), 256, 1);
    
    
    epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == -1, "epoll_create1");

    doc_root_fd = open(doc_root, O_RDONLY);
    EXIT_ON(doc_root_fd == -1, "open(doc_root)");
    
    return OK;
}

static void usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    julia port www_dir\n");
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        usage();
        exit(-1);
    }

    int listen_fd = -1;
    unsigned short port = atoi(argv[1]);

#ifdef REUSE_PORT
#define NCORES  (8)
    int workers[NCORES];
    for (int i = 0; i < NCORES - 1; i++) {
        int pid = fork();
        if (pid == 0) {
            goto worker;
        } else {
            workers[i] = pid;
        }
    }
    workers[NCORES - 1] = getpid();
    /*
    cpu_set_t mask;
    for (int i = 0; i < NCORES; i++) {
        CPU_ZERO(&mask);
        CPU_SET(i, &mask);
        sched_setaffinity(workers[i], sizeof(cpu_set_t), &mask);
        //sched_getaffinity(NCORES[i], sizeof(cpu_set_t), &mask);
        
    }
    */
worker:
#endif

    listen_fd = startup(port);
    if (listen_fd < 0) {
        fprintf(stderr, "startup server failed\n");
        exit(-1);
    }

    server_init(argv[2]);
    
    printf("julia started...\n");
    printf("listening at port: %d\n", port);
    printf("doc root: %s\n", argv[2]);
    fflush(stdout);

    assert(add_listener(&listen_fd) != -1);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, 3000);
        if (nfds == -1) {
            EXIT_ON(errno != EINTR, "epoll_wait");
            continue;
        }

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
                }
                continue;
            }
            if (events[i].events & EPOLLIN) {
                // Receive request or data
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
    }

    close(listen_fd);
    return 0;
}
