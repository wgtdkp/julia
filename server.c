#include <stdio.h>
#include <stdlib.h>
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
    char* args;
    int args_len;
    char* version;
    int version_len;
    //entities
    char* host;
    int host_len;

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
static inline char* dup_tok(char* begin, char* end);
static int get_method(char* str, int len);
static int parse_request_line(int client, Request* request, Resource* resource);
static int parse_request_header(int client, Request* request);
static int parse_uri(char* sbegin, char* send, Request* request, Resource* resource);
static int fill_resource(Resource* resource, char* begin, char* end);
static void* handle_request(void* args);
static int handle_static(int client, Request* request, Resource* resource);
static int put_response(int client, Request* request, Resource* resource, Response* response);
static int startup(unsigned short* port);
static const char* status_repr(int status);
static void setup_env(Request* request, const char* script_path);
static void cat(int des, int src);
static void catn(int des, int src, int n);
static const char* get_type(const char* ext);

static String* create_string(int c);
static String* destroy_string(String** str);
static void string_push_back(String* str, char ch);

// param: c: reserve c  bytes
String* create_string(int c)
{
    String* str = (String*)malloc(sizeof(String));
    str->size = 0;
    if (c == 0)
        str->data = NULL;
    str->data = (char*)malloc(c * sizeof(char));
    str->cap = c;
    return str;
}

String* destroy_string(String** str)
{
    free((*str)->data);
    free(*str);
    *str = 0;
}

void string_push_back(String* str, char ch)
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
    req->args = NULL;
    req->args_len = 0;
    req->version = NULL;
    req->version_len = 0;
    req->host = NULL;
    req->host_len = 0;
    return req;
}

static void destroy_request(Request** request)
{
    free((*request)->args);
    free((*request)->version);
    free((*request)->host);
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

static inline char* dup_tok(char* begin, char* end)
{
    int len = end - begin;
    char* ret = (char*)malloc((len + 1) * sizeof(char));
    memcpy(ret, begin, len);
    ret[len] = 0;
    return ret;
}

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
    if (len == -1)
        return -1;
    char* begin = buf;
    char* end = buf;
    WALK(begin, end);
    request->method = get_method(begin, end - begin);
    if (request->method < 0)
        return -1;
    
    WALK(begin, end);

    parse_uri(begin, end, request, resource);
    WALK(begin, end);
    request->version = dup_tok(begin, end);
    return 0;
}

//return: error
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
        if (0 == strncasecmp("Host", begin, end - begin)) {
            WALK(begin, end);   //skip ':'
            WALK(begin, end);
            request->host = dup_tok(begin, end);
        } else if (0 == strncasecmp("Content-Length", begin, end - begin)) {
            WALK(begin, end);   //skip ':'
            WALK(begin, end);
            request->content_length = atoi(begin);
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
    } else { //resource is a file
        //end = &resource->path[resource->path_len - 1];
        //while (*end != '.' && *end != '/') --end;
        //if (*end == '/') { 
        //    /*
        //     * the file request has no extension,
        //     * treat it as usual file
        //     */
        //    resource->type = RT_FILE;
        //} else {
        //    resource->type = get_type(end + 1);
        //}
    }

    return 0;
}

//return: error
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
        request->args = dup_tok(begin + 1, end);
    else
        request->args = NULL;

    *send = tmp;
    return 0;
}

static int get_line(int client, char* buf, int size)
{
    int i;
    int cnt = 0;
    for (i = 0; i < size; i++) {
        if (1 != (cnt = recv(client, &buf[i], 1, 0)) 
            || '\n' == buf[i]) {
            break;
        }
    }
    if (cnt == -1)
        return -1;
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
        cat(client, output[0]);

        close(output[0]);
        close(input[1]);
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
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
        snprintf(env_string, 256 - 1, "QUERY_STRING=%s", request->args);
        putenv(env_string);
    } else if (request->method == M_POST) {
        snprintf(env_length, 64 - 1, "CONTENT_LENGTH=%d", request->content_length);
        putenv(env_length);
    }
}

/*
 * read from src file to des file
 */
static void cat(int des, int src)
{
    // 500KiB buffer
    static const int buf_size = 500 * 1024;
    char buf[buf_size];
    int len;
    do {
        len = read(src, buf, buf_size);
        write(des, buf, len);
    } while (len > 0);
}

/*
 * read n bytes from src file to des file
 */
static void catn(int des, int src, int n)
{
    // 500KiB buffer
    static const int buf_size = 500 * 1024;
    char buf[buf_size];
    int len;
    do {
        len = read(src, buf, buf_size);
        write(des, buf, len);
        n -= len;
    } while (n > 0);
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
        send(client, begin, buf - begin, 0);
        return handle_cgi(client, request, resource);
    }
    /*
     * if has content
     */
    if (response->content_fd != -1) {
        buf += sprintf(buf, "Contend-Type: %s \r\n",
                response->content_type);
    }

    buf += sprintf(buf, "\r\n");
    send(client, begin, buf - begin, 0);
    
    /*
     * send content, if has
     */
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

static void* handle_request(void* args)
{
    int client = *((int*)args);
    //Buffer* request_buf = create_buffer(100);
    Request* request = create_request();
    Resource* resource = create_resource();
    if (0 != parse_request_line(client, request, resource))
        goto close;
    if (0 != parse_request_header(client, request))
        goto close;

    Response response = default_response;
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

    // send response
    put_response(client, request, resource, &response);

    // release resource
    if (response.content_fd != -1) {
        int err = close(response.content_fd);
        assert(err == 0);
    }

close:
    destroy_resource(&resource);
    destroy_request(&request);
    close(client);
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

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
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
}

static void usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    ./julia port www_dir\n");
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
    printf("doc root: %s\n", argv[2]);
    printf("listening at port: %d\n", port);
    fflush(stdout);
    while (1) {
        client_sock = accept(server_sock,
            (struct sockaddr*)&client, &client_len);
        if (client_sock == -1) {
            fprintf(stderr, "client connection failed\n");
        } else {
            pthread_t request_handler;
            int err = pthread_create(&request_handler,
                NULL, handle_request, &client_sock);
            if (err) {
                perror("pthread_create");
            }
        }
    }

    close(server_sock);
    return 0;
}
