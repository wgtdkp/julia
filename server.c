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
static const char* ext_map[][2] = {
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
static const size_t ext_num = sizeof(ext_map) / sizeof(ext_map[0]);

typedef unsigned char bool;
static const int true = 1;
static const int false = 0;

typedef struct {
    int cap;
    int size;
    char* data;
} String;

typedef enum {
    M_GET,
    M_POST,
    M_UNIMP,    //below is not implemented
    M_PUT,
    M_DELETE,
    M_TRACE,
    M_CONNECT,
    M_HEAD,
    M_OPTIONS,
} Method;

typedef struct {
    int method;
    //char* args;
    //int args_len;
    int version[2];
    String* query_string;
    //entities
    //char* host;
    //int host_len;
    /*
     * the reuqest header has entity 'connection:keep-alive',
     * current connection will be closed when recieved request
     */
    bool keep_alive;

    int content_length;
} Request;

typedef enum {
    RS_OK,
    RS_NOTFOUND,
    RS_DENIED, //resource access denied
} ResourceStat;

typedef struct {
    //the abs path to the resource
    char path[255];
    int path_len;
    //type: static file or script
    //ResourceType type;
    //the status of the resource
    ResourceStat stat;
} Resource;

typedef enum {
    MT_TEXT,
    MT_HTML,
    MT_CSS,
    MT_PLAIN,
    /* image */
    MT_IMG,
    MT_PNG,
    MT_JPG,
} MediaType;

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


static int get_line(int client, char* buf, int size);
static inline int empty_line(char* line, int len);
static Request* create_request(void);
static void destroy_request(Request** request);
static Resource* create_resource(void);
static void destroy_resource(Resource** resource);
static int get_method(char* str, int len);
static int parse_request_line(int client, Request* request, Resource* resource);
static int parse_request_header(int client, Request* request);
static int parse_uri(char* sbegin, char* send, Request* request, Resource* resource);
static int fill_resource(Resource* resource, char* begin, char* end);
static void* handle_request(int client);
static int put_response(int client, Request* request, Resource* resource, Response* response);
static int startup(unsigned short* port);
static const char* status_repr(int status);
static void setup_env(Request* request, const char* script_path);
static int transfer_chunk(int des, int src);
static int cat(int des, int src);
static int catn(int des, int src, int n);
static int cats(int des, const char* src, int n);
static const char* get_type(const char* ext);
static void ju_log(const char* format, ...);

static String* create_string(int c);
static String* destroy_string(String** str);
static void string_push_back(String* str, char ch);

// param: c: reserve c  bytes
static String* create_string(int c)
{
    String* str = (String*)malloc(sizeof(String));
    str->size = 0;
    if (c == 0)
        str->data = NULL;
    str->data = (char*)malloc(c * sizeof(char));
    memset((void*)str->data, 0, c * sizeof(char));
    str->cap = c;
    return str;
}

static String* destroy_string(String** str)
{
    free((*str)->data);
    free(*str);
    *str = 0;
}

static void string_push_back(String* str, char ch)
{
    if (str->size >= str->cap - 1) {
        int new_cap = str->cap * 2;
        char* new_data = (char*)malloc(new_cap * sizeof(char));
        if (str->size != 0)
            memcpy(new_data, str->data, str->size);
        new_data[str->size++] = ch;
        new_data[str->size] = 0;
        free(str->data);
        str->data = new_data;
        str->cap = new_cap;
        ++str->size;
    } else {
        str->data[str->size++] = ch;
        str->data[str->size] = 0;
    }
}

static int string_append(String* str, const char* p, int len)
{   

    return 0;
}

static int string_print(String* str, const char* format, ...)
{
    return 0;
}

static void string_reserve(String* str, int n)
{
    if (n <= str->cap)
        return;
    str->data = (char*)malloc(n * sizeof(char));
    memset((void*)str->data, 0, n * sizeof(char));
    str->cap = n;
}


static inline int is_script(const char* ext)
{
    if (strncasecmp("php", ext, 3) == 0)
        return 1;
    return 0;
}

static Request* create_request(void)
{
    Request* req = (Request*)malloc(sizeof(Request));
    req->method = M_GET;
    req->version[0] = req->version[1] = 1;
    req->query_string = create_string(32);
    req->keep_alive = false;
    return req;
}

static void destroy_request(Request** request)
{
    destroy_string(&(*request)->query_string);
    free(*request);
    *request = NULL;
}

static Resource* create_resource(void)
{
    Resource* res = (Resource*)malloc(sizeof(Resource));
    strcpy(res->path, www_dir);
    res->path_len = strlen(res->path);
    res->stat = RS_OK;
    return res;
}

static void destroy_resource(Resource** resource)
{
    free(*resource);
    *resource = NULL;
}

/*
static inline char* dup_tok(char* begin, char* end)
{
    int len = end - begin;
    char* ret = (char*)malloc((len + 1) * sizeof(char));
    memcpy(ret, begin, len);
    ret[len] = 0;
    return ret;
}
*/

static inline int empty_line(char* line, int len)
{
    return (len >= 2 && line[0] == '\r' && line[1] == '\n') \
        || (len == 1 && line[0] == '\n');
}

static int get_method(char* str, int len)
{
    if (0 == strncasecmp("GET", str, len))
        return M_GET;
    else if (0 == strncasecmp("POST", str, len))
        return M_POST;
    else if (0 == strncasecmp("PUT", str, len))
        return M_PUT;
    else if (0 == strncasecmp("DELETE", str, len))
        return M_DELETE;
    else if (0 == strncasecmp("TRACE", str, len))
        return M_TRACE;
    else if (0 == strncasecmp("CONNECT", str, len))
        return M_CONNECT;
    else if (0 == strncasecmp("HEAD", str, len))
        return M_HEAD;
    else if (0 == strncasecmp("OPTIONS", str, len))
        return M_OPTIONS;
    else {
        int tmp = str[len];
        str[len] = 0;
        fprintf(stderr, "unexpected method: %s\n", str);
        str[len] = tmp;
        return -1;
    }
}

static const char* method_repr(Method method)
{
    switch (method) {
    case M_GET: return "GET";
    case M_POST: return "POST";
    case M_OPTIONS: return "OPTIONS";
    case M_HEAD: return "HEAD";
    case M_CONNECT: return "CONNECT";
    case M_TRACE: return "TRACE";
    case M_DELETE: return "DELETE";
    case M_PUT: return "PUT";
    default: return "*";
    }
}

//return error
static int parse_request_line(int client, Request* request, Resource* resource)
{
    char buf[1024];
    int len = get_line(client, buf, sizeof(buf));
    //fprintf(stderr, "request line length: %d\n", len);
    if (len == -1)
        return -1;
    char* begin = buf;
    char* end = buf;
    WALK(begin, end);
    if (*begin == 0)
        return -1;
    request->method = get_method(begin, end - begin);
    if (request->method < 0)
        return -1;
    
    WALK(begin, end);

    parse_uri(begin, end, request, resource);
    WALK(begin, end);
    if (0 == strncasecmp("HTTP/1.1", begin, end - begin)) {
        request->version[0] = request->version[1] = 1;
    } else if (0 == strncasecmp("HTTP/1.1", begin, end - begin)) {
        request->version[0] = 1;
        request->version[1] = 0;
    }
    return 0;
}

//return: -1, error;
static int parse_request_header(int client, Request* request)
{
    char buf[1024];
    while (1) {
        int len = get_line(client, buf, sizeof(buf));
        if (len == -1) return -1;
        if (0 == len) break; // '\r\n' has already been trimed out
        char* begin = buf;
        char* end = buf;
        WALK_UNTIL(end, ':');
        if (0 == strncasecmp("Content-Length", begin, end - begin)) {
            WALK(begin, end);   //skip ':'
            WALK(begin, end);
            request->content_length = atoi(begin);
        } else if (0 == strncasecmp("connection", begin, end - begin)) {
            WALK(begin, end);
            WALK(begin, end);
            if (request->version[0] == 1 && request->version[1] == 0) {
                request->keep_alive = false;
                if (0 == strncasecmp("keep-alive", begin, end - begin))
                    request->keep_alive = true;
            } else {
                request->keep_alive = true;
                if (0 == strncasecmp("close", begin, end - begin))
                    request->keep_alive = false;
            }
        }
        //TODO(wgtdkp): handle other header entities
    }
    return 0;
}

//return the new length of des
static inline int tokcat(char* des, int des_len, char* src_begin, char* src_end)
{
    int ret = des_len + (src_end - src_begin);
    memcpy(des + des_len, src_begin, src_end - src_begin);
    des[ret] = 0;
    return ret;
}

static int fill_resource(Resource* resource, char* sbegin, char* send)
{
    if (sbegin == send) { //path not explicitly given
        sbegin = "/";
        send = sbegin + 1;
    }
    //TODO(wgtdkp): check if token(begin, end) is too long
    resource->path_len = 
        tokcat(resource->path, resource->path_len, sbegin, send);

    char* begin = sbegin;
    char* end = sbegin;
    
    /*
     * skip the first '/'(there is always '/' at the beginning)
     */
    WALK_UNTIL(end, '/');
    begin = ++end;
    
    /*
     * check if request resource out of www_dir,
     * that is: the '../' is more than other entries(except './') until now.
     */
    int ddots = 0, dentries = 0;
    while (1) {
        WALK_UNTIL(end, '/');
        if (IS_DDOT(begin, end))
            ++ddots;
        else if (end - begin > 0 && !IS_DOT(begin, end))
            ++dentries;
        if (ddots > dentries) {
            resource->stat = RS_DENIED;
            return 0;
        }
        if (*end == 0)
            break;
        begin = ++end;
    }
    resource->stat = RS_OK;

    struct stat st;
check:
    if (stat(resource->path, &st) == -1) {
        resource->stat = RS_NOTFOUND;
    } else if (S_ISDIR(st.st_mode)) {
        /*
         * it's ok to cat '/' to path, 
         * even if path is ended by '/'
         */
        strcat(resource->path, "/");
        strcat(resource->path, default_index);
        resource->path_len += 1 + strlen(default_index);
        goto check;
    } else {} //resource is a file

    return 0;
}

//return: -1, error;
static int parse_uri(char* sbegin, char* send, Request* request, Resource* resource)
{   

    char http[] = "http://";
    int http_len = strlen(http);
    if (0 == strncasecmp(http, sbegin, http_len))
        sbegin += http_len;
    int tmp = *send;
    *send = 0;

    WALK_UNTIL(sbegin, '/');
    char* begin = sbegin;
    WALK_UNTIL(sbegin, '?');
    char* end = sbegin;
    fill_resource(resource, begin, end);

    begin = end;
    WALK(begin, end);
    if (begin != end)
        string_append(request->query_string, begin + 1, end - (begin + 1));

    *send = tmp;
    return 0;
}

static int get_line(int client, char* buf, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        // TODO(wgtdkp): read mutlibytes to speed up
        if (1 != read(client, &buf[i], 1))
            return -1;
        if ('\n' == buf[i])
            break;
    }
    /*
     *trim '\r\n'
     */
    if (buf[i] == '\n')
        buf[i] = 0;
    if (i >= 1 && buf[i - 1] == '\r')
        buf[--i] = 0;
    //DEBUG(buf);
    return i;
}

static int handle_cgi(int client, Request* request, Resource* resource)
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
        //fprintf(stderr, "chunk-size: %d\n", readed);
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
    // TODO(wgtdkp): it may be not possible for big file to be mapped into memory
    char* addr = mmap(NULL, len, PROT_READ, MAP_SHARED, src, 0);
    assert(addr != MAP_FAILED);
    // TODO(wgtdkp): handle partial write
    assert(write(des, addr, len) == len);
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

static const char* get_type(const char* ext)
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
            //DEBUG("close connection");
            close(client);
            return 0;
        }
        //DEBUG("sending done...");
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

static void ju_log(const char* format, ...)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char file_name[64];
    snprintf(file_name, 64 - 1, "log-%d-%d-%d.txt",
            tm.tm_year, tm.tm_mon + 1, tm.tm_mday);

    FILE* log_file = fopen(file_name, "a+");
    if (log_file == NULL)
        return;

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fclose(log_file);
}

#define EXIT_ON(cond, msg)  if ((cond)) { perror((msg)); exit(EXIT_FAILURE); }

static inline void set_nonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    EXIT_ON(flag == -1, "fcntl: F_GETFL");
    flag |= O_NONBLOCK;
    EXIT_ON(fcntl(fd, F_SETFL, flag) == -1, "fcntl: FSETFL");
}

int main(int argc, char* argv[])
{
    // 0. arg parsing
    if (argc < 3) {
        usage(); exit(-1);
    }
    config(argv[2]);

    // 1.config
    int server_sock = -1;
    int client_sock = -1;
    unsigned short port = atoi(argv[1]);
    struct sockaddr_in client;
    int client_len = sizeof(client);

    // 2.start server
    server_sock = startup(&port);
    if (server_sock < 0) {
        fprintf(stderr, "startup server failed: %d\n", server_sock);
        exit(-1);
    }

    // 3. listening loop
    printf("julia started...\n");
    printf("doc root: %s\n", www_dir);
    printf("listening at port: %d\n", port);
    fflush(stdout);

    const int nevents = 128;
    struct epoll_event ev, events[nevents];
    int epoll_fd = epoll_create1(0);
    EXIT_ON(epoll_fd == -1, "epoll_createl");

    ev.events = EPOLLIN;
    ev.data.fd = server_sock;
    EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &ev) == -1,
            "epoll_ctl: server_sock");
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, nevents, -1);
        EXIT_ON(nfds == -1, "epoll_wait");
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_sock) {
                client_sock = accept(server_sock,
                        (struct sockaddr*)&client, &client_len);
                EXIT_ON(client_sock == -1, "accept");
                set_nonblocking(client_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_sock;
                EXIT_ON(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) == -1,
                        "epoll_ctl: client_sock");
            } else {
                handle_request(events[i].data.fd);
            }
        }
    }

    /*
    pthread_t request_handler;
    int i = 0;
    while (1) {
        client_sock = accept(server_sock,
            (struct sockaddr*)&client, &client_len);
        if (client_sock == -1) {
            fprintf(stderr, "client connection failed\n");
        } else {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            //ju_log("[%d:%d:%d] %s: ", tm.tm_hour, tm.tm_min, tm.tm_sec, 
            //        inet_ntoa(client.sin_addr));
            //fprintf(stderr, "client[%d]: socket: %d\n", i++, client_sock);
            
            int err = pthread_create(&request_handler,
                NULL, handle_request, client_sock);
            if (err) {
                perror("pthread_create");
                break;
            }
            if (++i == 20000)
                break;
        }
    }
    */
    close(server_sock);
    return 0;
}
