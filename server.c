#include "server.h"

int root_fd;
static pid_t worker_pid;
static clock_t total = 0;
//static int max_connection = 0;
//static int max_response = 0;

static int startup(unsigned short port);
static int server_init(char* cfg_file);
static void usage(void);

static void sig_int(int signo) {
    //float total_time = 1.0f * total / CLOCKS_PER_SEC;
    //printf("[%d]: time: %f, total reqs: %d, RPS: %f\n",
    //        getpid(), total_time, total_reqs, total_reqs / total_time);
    //printf("connection pool allocated: %d\n", connection_pool.nallocated);
    //printf("max c allocated: %d\n", max_connection);
    //printf("resquest pool allocated: %d\n", request_pool.nallocated);
    //printf("max response allocated: %d\n", max_response);
    printf("julia exited...\n");
    fflush(stdout);
    if (worker_pid != getpid())
        kill(worker_pid, SIGKILL);
    exit(0);
}

static int startup(uint16_t port) {
    // If the client closed the c, then it will cause SIGPIPE
    // Here simplely ignore this SIG
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_int);
    
    // Open socket, bind and listen
    int listen_fd = 0;
    struct sockaddr_in server_addr = {0};
    int addr_len = sizeof(server_addr);
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == ERROR) {
        return ERROR;
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
        return ERROR;
    }

    // TODO(wgtdkp): make parameter 'backlog' configurable?
    if (listen(listen_fd, 1024) < 0) {
        return ERROR;
    }
    return listen_fd;
}

static int server_init(char* cfg_file) {
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
    pool_init(&request_pool, sizeof(request_t), 8, 0);
    //pool_init(&accept_pool, LIST_WIDTH(accept_type_t), 4, 0);
    
    
    epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == ERROR, "epoll_create1");

    root_fd = open(server_cfg.root.data, O_RDONLY);
    EXIT_ON(root_fd == ERROR, "open(root)");
    
    return OK;
}

static void accept_connection(int listen_fd) {
    while (true) {
        int c_fd = accept(listen_fd, NULL, NULL);
        if (c_fd == ERROR) {
            // TODO(wgtdkp): handle this error
            // There could be too many connections(beyond OPEN_MAX)
            ERR_ON((errno != EWOULDBLOCK), "accept");
            break;
        }
        open_connection(c_fd);
    }
}

static void usage(void) {
    fprintf(stderr, "Usage:\n"
                    "    julia config_file\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        exit(ERROR);
    }
    worker_pid = getpid();

#ifdef REUSE_PORT
#define NCORES  (8)
    int workers[NCORES];
    for (int i = 0; i < NCORES - 1; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            goto work;
        } else {
            workers[i] = pid;
        }
    }
    workers[NCORES - 1] = getpid();
    cpu_set_t mask;
    for (int i = 0; i < NCORES; ++i) {
        CPU_ZERO(&mask);
        CPU_SET(i, &mask);
        sched_setaffinity(workers[i], sizeof(cpu_set_t), &mask);
        //sched_getaffinity(NCORES[i], sizeof(cpu_set_t), &mask);
    }
work:
#elif !defined DEBUG
    while (1) {
        int stat;
        worker_pid = fork();
        if (worker_pid < 0)
            perror("fork");
        else if (worker_pid > 0) {
            wait(&stat);
            if (WIFEXITED(stat))
                exit(ERROR);
            // Worker unexpectly exited, restart it
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            ju_error("%d-%d-%d %d:%d:%d: error occurred...\n"
                          "restarting...\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
        } else {
            break;
        }
    }
#endif
    int listen_fd = ERROR;

    if (server_init(argv[1]) != OK)
        return ERROR;
    
    listen_fd = startup(server_cfg.port);
    if (listen_fd < 0) {
        fprintf(stderr, "startup server failed\n");
        exit(ERROR);
    }
    
    printf("julia started...\n");
    printf("listening at port: %u\n", server_cfg.port);
    print_string("doc root: %*s\n", &server_cfg.root);

    assert(add_listener(&listen_fd) != ERROR);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, 30);
        if (nfds == ERROR) {
            EXIT_ON(errno != EINTR, "epoll_wait");
        }
        
        clock_t begin = clock();
        //TODO(wgtdkp): multithreading here: seperate fds to several threads
        for (int i = 0; i < nfds; ++i) {
            int fd = *((int*)(events[i].data.ptr));
            if (fd == listen_fd) {
                // We could accept more than one c per request
                accept_connection(listen_fd);
                continue;
            }
            int err;
            connection_t* c = events[i].data.ptr;
            if (events[i].events & EPOLLIN) {
                err = (c->side == C_SIDE_BACK) ?
                      handle_upstream(c): handle_request(c);
                if (err == ERROR) {
                    close_connection(c);
                    bzero(&events[i], sizeof(events[i]));
                }
            }
            
            if (events[i].events & EPOLLOUT) {
                err = (c->side == C_SIDE_BACK) ? 
                      handle_pass(c): handle_response(c);
                if (err == ERROR) {
                    close_connection(c);
                    bzero(&events[i], sizeof(events[i]));
                }
            }
        }
        total += clock() - begin;
    }

    close(listen_fd);
    return 0;
}
