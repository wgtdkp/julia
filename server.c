#include "connection.h"
#include "request.h"
#include "response.h"
#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define DEBUG(msg)  fprintf(stderr, "%s\n", (msg));

/*
static char default_index[] = "index.php";
static char default_404[256];
static char default_403[256];
static char default_unimpl[256];
static char www_dir[256];
*/

//typedef unsigned char bool;
//static const int true = 1;
//static const int false = 0;

static const Response default_response = {
    .status = 200,
    .content_type = "text/html",
    .content_fd = -1,
    .is_script = 0
};

static int startup(unsigned short port);
/*
static void setup_env(Request* request);
static int transfer_chunk(int des, int src);
static int cat(int des, int src);
static int catn(int des, int src, int n);
static int cats(int des, const char* src, int n);
*/
void* handle_request(Connection* connection);
int put_response(Connection* connection);;

/*
static inline int is_script(const char* ext)
{
    if (strncasecmp("php", ext, 3) == 0)
        return 1;
    return 0;
}
*/

static void send_test_response(Connection* connection)
{
    int fd = connection->fd;
    // TODO(wgtdkp): try to understanding this setting!!!
    // If set, don't send out partial frames. 
    // All queued partial frames are sent when the option is cleared again.
    // This is useful for prepending headers before calling sendfile(2), 
    // or for throughput optimization
    int on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));

    char buffer[1024]; 
    sprintf(buffer, "HTTP/1.0 200 OK\r\n"
                   "Server: julia\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: Keep-Alive\r\n"
                   "Content-Length: 997\r\n\r\n");
    send(fd, buffer, strlen(buffer), 0);
    memset(buffer, 65, 997);
    send(fd, buffer, 997, 0);
    //close(fd);
    //delete_connection(connection);
    on = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));
}

void* handle_request(Connection* connection)
{
    int fd = connection->fd;
    char buffer[1024];
    read(fd, buffer, 1024);
    send_test_response(connection);

    //event_set_to(connection, EVENTS_OUT);
/*
    //int client = *((int*)args);
    while (true) {
        bool keep_alive = false;
        //Buffer* request_buf = create_buffer(100);
        Request* request = create_request();
        Resource* resource = create_resource();
        if (0 != parse_request_line(client, request, resource))
            goto close;
        if (0 != parse_request_header(client, request))
            goto close;

        keep_alive = request->keep_alive;
        //ju_log("%s %s \n", method_repr(request->method), resource->path);

        Response response = default_response;

        if (request->method != M_GET && request->method != M_POST) {
            response.status = 501;
            response.content_type = "text/html";
            response.content_fd = open(default_unimpl, O_RDONLY, 00777);
            goto put;
        }

        const char* ext = get_extension(resource);
        if (is_script(ext)) {
            response.is_script = 1;
            response.content_type = "text/html";
        } else {
            response.is_script = 0;
            // TODO(wgtdkp): support more MIME types
            // content_type could be null
            response.content_type = get_type(ext);
        }

        switch(resource->stat) {
        case RS_OK:
            response.status = 200;
            if (!response.is_script) {
                response.content_fd = open(resource->path, O_RDONLY, 00777);
                assert(response.content_fd != -1);
            }
            break;
        case RS_DENIED:
            response.status = 403;
            response.content_fd = open(default_403, O_RDONLY, 00777);
            break;
        case RS_NOTFOUND:
        default:
            response.status = 404;
            response.content_fd = open(default_404, O_RDONLY, 00777);
            break;
        }
    put:
        // send response
        put_response(client, request, resource, &response);

        // release resource
        if (response.content_fd != -1) {
            int err = close(response.content_fd);
            assert(err == 0);
        }
        
    close:;
        destroy_resource(&resource);
        destroy_request(&request);
        if (!keep_alive){
            close(client);
            return 0;
        }
    }
*/
    return NULL;
}


// TODO(wgtdkp): use sendfile() for static resource
int put_response(Connection* connection)
{
    assert(0);
    send_test_response(connection);
    //event_set_to(connection, EVENTS_IN);
/*
    char begin[1024];
    char* buf = begin;
    buf += sprintf(buf, "HTTP/%1d.%1d %3d %s \r\n", 
            http_version[0],
            http_version[1],
            response->status,
            status_repr(response->status));
    buf += sprintf(buf, "server: julia/0.0.0 \r\n");

    if (response->is_script && response->status == 200) {
        buf += sprintf(buf, "transfer-encoding: chuncked \r\n");
        write(client, begin, buf - begin);
        return handle_cgi(client, request, resource);
    }

    // If has content
     buf += sprintf(buf, "contend-type: %s \r\n",
                response->content_type);
    if (response->content_fd != -1) {
        struct stat fst;
        fstat(response->content_fd, &fst);
        int size = fst.st_size;
        buf += sprintf(buf, "content-length: %d \r\n", size);
    } else {
        buf += sprintf(buf, "content-length: %d \r\n", 0);
    }


    buf += sprintf(buf, "\r\n");
    write(client, begin, buf - begin);
    
    // Send content, if has
    if (response->content_fd != -1) {
        cat(client, response->content_fd);
    }
*/
    return 0;
}

/*
static int handle_cgi(int client, Request* request)
{
    int pid;
    int input[2];
    int output[2];
    if (pipe(output) < 0)
        perror("pipe(output)");
    if (pipe(input) < 0)
        perror("pipe(input)");
    if ((pid = fork()) < 0)
        perror("fork");

    if (pid == 0) { // child, execute the script
        dup2(output[1], STDOUT_FILENO);
        dup2(input[0], STDIN_FILENO);
        close(input[1]);
        close(output[0]);
     
        setup_env(request, resource->path);
        // TODO(wgtdkp): handle more scripts
        execv("/usr/bin/php5-cgi", (char*[]){NULL});
        perror("php5-cgi");
    } else {
        close(output[1]);
        close(input[0]);
        if (request->method == M_POST) {
            catn(input[1], client, request->content_length);
        }
        //cat(client, output[0]);
        // TODO(wgtdkp): transfer chunked
        //DEBUG("sending chunk...");
        transfer_chunk(client, output[0]);

        close(output[0]);
        close(input[1]);
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}
*/

/*
static int transfer_chunk(int des, int src)
{
    int total = 0;
    char buf[1024 * 1024];
    while (true) {
        int size = 1024 * 1024;
        int readed = 0;
        do {
            int len = read(src, buf + readed, size);
            if (len < 0) return readed; // error
            else if (len == 0) break;
            readed += len;
            size -= len;
        } while (true);
        if (readed <= 0) // finish reading file
            break;
        char chunk_len[12];
        int len = sprintf(chunk_len, "%x\r\n", readed);
        assert(len == write(des, chunk_len, len));
        cats(des, buf, readed);
        write(des, "\r\n", 2);
        total += readed;
    }
    return total;
}
*/

/*
static int cats(int des, const char* src, int n)
{
    int readed = 0;
    do {
        int len = write(des, src + readed, n);
        if (len < 0) break;
        n -= len;
        readed += len;
    } while (n > 0);
    return readed;
}
*/

/*
static void setup_env(Request* request, const char* script_path)
{
    // [POS34-C]: Do not call putenv() with a pointer to an automatic variable as the argument
    // as putenv() just do copy the pointer! so each enrivonment needs a buffer.
    static char env_filename[256];
    static char env_method[64];
    static char env_string[256];
    static char env_length[64];

    snprintf(env_filename, 256 - 1, "SCRIPT_FILENAME=%s", script_path);
    putenv(env_filename);
    putenv("REDIRECT_STATUS=200");
    putenv("GATEWAY_INTERFACE=CGI/1.1");
    putenv("CONTENT_TYPE=application/x-www-form-urlencoded");
    
    snprintf(env_method, 64 - 1, "REQUEST_METHOD=%s", method_repr(request->method));
    putenv(env_method);
    if (request->method == M_GET) {
        snprintf(env_string, 256 - 1, "QUERY_STRING=%s", request->query_string.data);
        putenv(env_string);
    } else if (request->method == M_POST) {
        snprintf(env_length, 64 - 1, "CONTENT_LENGTH=%d", request->content_length);
        putenv(env_length);
    }
}
*/

/*
// Read from src file to des file
static int cat(int des, int src)
{
    struct stat st;
    assert(fstat(src, &st) != -1);
    int len = st.st_size;
    if (len == 0)
        return 0;
    // TODO(wgtdkp): it may be not possible for big file to be mapped into memory
    char* addr = mmap(NULL, len, PROT_READ, MAP_PRIVATE, src, 0);
    assert(addr != MAP_FAILED);
    // TODO(wgtdkp): handle partial write
    assert(cats(des, addr, len) == len);
    munmap(addr, len);
    return len;
}
*/

/*
// Read n bytes from src file to des file
static int catn(int des, int src, int n)
{
    // 500KiB buffer
    static const int buf_size = 500 * 1024;
    char buf[buf_size];
    int len, readed = 0;
    do {
        int size = MIN(buf_size, n);
        len = read(src, buf, size);
        if (len < 0) break;
        write(des, buf, len);
        readed += len;
        n -= len;
    } while (n > 0);    assert(n == 0);
    return readed;
}
*/


static int startup(unsigned short port)
{
    // TODO(wgtdkp): try to understanding this func call!!!
    // If the client closed the connection, then it will cause IGPIPE
    // Here simplely ignore this SIG
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = 0;
    struct sockaddr_in server_addr = {0};
    int addrlen = sizeof(server_addr);
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        return -1;
    }
    memset((void*)&server_addr, 0, addrlen);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr*)&server_addr, addrlen) < 0) {
        return -1;
    }

    if (listen(listen_fd, 1024) < 0) {
        return -1;
    }
    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    return listen_fd;
}

/*
static void config(const char* dir)
{
    realpath(dir, www_dir);
    snprintf(default_404, 256 - 1, "%s/404.html", www_dir);
    snprintf(default_403, 256 - 1, "%s/403.html", www_dir);
    snprintf(default_unimpl, 256 - 1, "%s/unimpl.html", www_dir);
}
*/

static void usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    julia port www_dir\n");
}

/*
static int nconnections = 0;
static int nrequests = 0;
static int nresponses = 0;
static clock_t total_clocks = 0;
static int total_nfds = 0;
static int total_accepts = 0;
static int total_requests = 0;
*/
int main(int argc, char* argv[])
{
    if (argc < 3) {
        usage(); exit(-1);
    }
    //config(argv[2]);

    int listen_fd = -1;
    unsigned short port = atoi(argv[1]);

    listen_fd = startup(port);
    if (listen_fd < 0) {
        fprintf(stderr, "startup server failed\n");
        exit(-1);
    }

    printf("julia started...\n");
    //printf("doc root: %s\n", www_dir);
    printf("listening at port: %d\n", port);
    fflush(stdout);

    epoll_init();
    event_add_listen(&listen_fd);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, 3000);
        if (nfds == -1) {
            EXIT_ON(errno != EINTR, "epoll_wait");
            continue;
        }
        for (int i = 0; i < nfds; i++) {
            // Here is a hacking: 
            // Eeven if events[i].data is set to pointer of connection,
            // we can get it's fd correctly(as 'fd' is the first member of struct Connection).
            int fd = *((int*)(events[i].data.ptr));
            if (fd == listen_fd) {
                while (true) {
                    struct sockaddr_in client;
                    socklen_t client_len = sizeof(client);
                    int connection_fd = accept(fd,
                            (struct sockaddr*)&client, &client_len);
                    if (connection_fd == -1) {
                        EXIT_ON((errno != EAGAIN) && (errno != EWOULDBLOCK),
                                "accept");
                        break;
                    }
                    Connection* connection = new_connection(connection_fd);
                    event_add(connection, EVENTS_IN);
                }
            } else if (events[i].events & EPOLLIN) {
                // Receive request or data
                Connection* connection = (Connection*)(events[i].data.ptr);
                handle_request(connection);
            } else if (events[i].events & EPOLLOUT) {
                // Send response
                Connection* connection = (Connection*)(events[i].data.ptr);
                put_response(connection);
            } else {
                // Neither epoll in or out event
                EXIT_ON(true, "epoll_wait");
            }
        }
    }

    close(listen_fd);
    return 0;
}
