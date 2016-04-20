#include "connection.h"
#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <strings.h>


#define DEBUG(msg)  fprintf(stderr, "%s\n", (msg));

#define MIN(x, y)   ((x) > (y) ? (y): (x))
#define MAX(x, y)   MIN(y, x)

#define WALK(begin, end)    while (*end != 0 && isspace((int)(*end))) { ++end; } \
                            begin = end;  \
                            while (*end != 0 && !isspace((int)(*end))) { ++end; }

#define WALK_UNTIL(begin, ch)   while (*begin != 0 && *begin != ch) ++begin;

#define IS_DOT(begin, end)  ((end) - (begin) == 1 && *(begin) == '.')
#define IS_DDOT(begin, end) ((end) - (begin) == 2 && *(begin) == '.' && *((begin) + 1) == '.')

/*
 * root directory of the application;
 * any resource that out of this directory
 * is not available;
*/
static char default_index[] = "index.php";
static char default_404[256];
static char default_403[256];
static char default_unimpl[256];
static char www_dir[256];
static char http_version[2] = {1, 1};   // http/1.1
static const char* mime_map[][2] = {
    {"css",     "text/css"},
    {"htm",     "text/html"},
    {"html",    "text/html"},
    {"gif",     "image/gif"},
    {"ico",     "image/x-icon"},
    {"jpeg",    "image/jpeg"},
    {"jpg",     "image/jpeg"},
    {"svg",     "image/svg+xml"},
    {"txt",     "text/plain"},
    {"zip",     "application/zip"},
};
static const size_t mime_num = sizeof(mime_map) / sizeof(mime_map[0]);

typedef unsigned char bool;
static const int true = 1;
static const int false = 0;

typedef struct {
    int status;
    const char* content_type;
    int content_fd;
    int is_script;
} Response;

static const Response default_response = {
    .status = 200,
    .content_type = "text/html",
    .content_fd = -1,
    .is_script = 0
};

static int parse_request_line(int client, Request* request);
static int parse_request_header(int client, Request* request);
static int parse_uri(char* sbegin, char* send, Request* request);
static int fill_resource(Resource* resource, char* begin, char* end);
static void* handle_request(int client);
static int put_response(int client, Response* response);
static int startup(unsigned short* port);
static const char* status_repr(int status);
static void setup_env(Request* request);
static int transfer_chunk(int des, int src);
static int cat(int des, int src);
static int catn(int des, int src, int n);
static int cats(int des, const char* src, int n);
static const char* get_mime_type(const char* ext);
static void ju_log(const char* format, ...);

static inline int is_script(const char* ext)
{
    if (strncasecmp("php", ext, 3) == 0)
        return 1;
    return 0;
}

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

// chunk size: 1MiB
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

static void setup_env(Request* request, const char* script_path)
{
    /*
     * [POS34-C]: Do not call putenv() with a pointer to an automatic variable as the argument
     * as putenv() just do copy the pointer! so each enrivonment needs a buffer.
     */
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
        snprintf(env_string, 256 - 1, "QUERY_STRING=%s", request->query_string->data);
        putenv(env_string);
    } else if (request->method == M_POST) {
        snprintf(env_length, 64 - 1, "CONTENT_LENGTH=%d", request->content_length);
        putenv(env_length);
    }
}

/*
 * read from src file to des file
 */
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

/*
 * read n bytes from src file to des file
 */
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
    } while (n > 0);
    assert(n == 0);
    return readed;
}

static int put_response(int client, Request* request, Resource* resource, Response* response)
{
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

    /*
     * if has content
     */
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
    
    /*
     * send content, if has
     */
    //DEBUG("ready to cat file...");
    
    if (response->content_fd != -1) {
        cat(client, response->content_fd);
    }
    return 0;
}

static const char* get_extension(Resource* resource)
{
    const char* end = &resource->path[resource->path_len-1];
    while (*end != '.' && *end != '/') --end;
    if (*end == '/') { 
         return &resource->path[resource->path_len-1];
    } else {
        return end + 1;
    }
}

static const char* get_mime_type(const char* ext)
{
    int begin = 0, end = ext_num;
    while (begin <= end) {
        int mid = (end - begin) / 2 + begin;
        int res = strcasecmp(ext, ext_map[mid][0]);
        if (res == 0)
            return ext_map[mid][1];
        else if (res > 0)
            begin = mid + 1;
        else
            end = mid - 1;
    }
    return NULL;
}

static void* handle_request(int client)
{
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
}

static const char* status_repr(int status)
{
    switch(status) {
#       include "status.inc"
    }
    return "(null)";
}

static int startup(unsigned short* port)
{
    int server_sock = 0;
    struct sockaddr_in server_addr = {0};
    int addrlen = sizeof(server_addr);

    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        return -1;
    }
    memset((void*)&server_addr, 0, addrlen);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(*port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_sock, (struct sockaddr*)&server_addr, addrlen) < 0) {
        return -2;
    }

    if (0 == *port) {
        if (-1 == getsockname(server_sock,
            (struct sockaddr*)&server_addr, &addrlen)) {
            return -3;
        }
        *port = ntohs(server_addr.sin_port);
    }
    if (0 > listen(server_sock, 1024)) {
        return -4;
    }

    return server_sock;
}

static void config(const char* dir)
{
    realpath(dir, www_dir);
    snprintf(default_404, 256 - 1, "%s/404.html", www_dir);
    snprintf(default_403, 256 - 1, "%s/403.html", www_dir);
    snprintf(default_unimpl, 256 - 1, "%s/unimpl.html", www_dir);
}

static void usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    ./julia port www_dir\n");
}

#define EXIT_ON(cond, msg)  if ((cond)) { perror((msg)); exit(EXIT_FAILURE); }

static inline void set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
}

// return: -1, error; else, accepted socket fd
static inline int event_add(int epoll_fd, int event_fd)
{
    struct epoll_event ev;
    set_nonblocking(event_fd);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = event_fd;
    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &ev) == -1,
            "epoll_ctl: event_fd");
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        usage(); exit(-1);
    }
    config(argv[2]);

    int server_sock = -1;
    int client_sock = -1;
    unsigned short port = atoi(argv[1]);

    server_sock = startup(&port);
    if (server_sock < 0) {
        fprintf(stderr, "startup server failed: %d\n", server_sock);
        exit(-1);
    }

    printf("julia started...\n");
    printf("doc root: %s\n", www_dir);
    printf("listening at port: %d\n", port);
    fflush(stdout);

    const int nevents = 128;
    struct epoll_event events[nevents];
    int epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == -1, "epoll_createl");

    event_add(epoll_fd, server_sock);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, nevents, -1);
        EXIT_ON(nfds == -1, "epoll_wait");
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_sock) {
                while (true) {
                    struct sockaddr_in client;
                    int client_len = sizeof(client);

                    int client_sock = accept(server_sock,
                            (struct sockaddr*)&client, &client_len);
                    if (client_sock == -1) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                            perror("accept");
                        break;
                    }
                    event_add(epoll_fd, client_sock);
                }
            } else {
                handle_request(events[i].data.fd);
            }
        }
    }

    close(server_sock);
    return 0;
}
