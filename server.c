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
 root directory of the application;
 any resource that out of this directory
 is not available;
*/
static char default_file[] = "index.html";
static char www_dir[255];
static char http_version[2] = {1, 1};   // http/1.1

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
} Request;

typedef enum {
    RT_FILE,
    RT_TEXT,
    RT_IMG,
    RT_PHP,
    RT_PY,
} ResourceType;

typedef enum {
    RS_OK,
    RS_NOTFOUND,
    RS_DENIED, //resource access denied
} ResourceStat;

typedef struct {
    //the abs path to the resource
    char path[255];
    int path_len;
    int size;
    //type: static file or script
    ResourceType type;
    //the status of the resource
    ResourceStat stat;
} Resource;

typedef struct {
    char* begin;
    char* end;
    char need_free;
} String;


typedef enum {
    MT_TEXT,
    MT_HTML,
    /* image */
    MT_IMG,
    MT_PNG,
    MT_JPG,
} MediaType;

typedef struct {
    int status;
    int content_encode;
    int content_length;
    MediaType content_type;
    MediaType content_subtype;
} ResponseHeader;

typedef struct {
    ResponseHeader header;
    char* content;
} Response;

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
static int handle_get(int client, Request* request, Resource* resource);
static int handle_post(int client, Request* request, Resource* resource);
static int put_response(int client, Response* response);
static int startup(unsigned short* port);
static const char* status_repr(int status);
static const char* content_type_repr(MediaType type);

static inline int is_static(ResourceType type)
{
    return type < RT_PHP;
}

static inline int is_script(ResourceType type)
{
    return !is_static(type);
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
    res->type = RT_TEXT;
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

//return error
static int parse_request_line(int client, Request* request, Resource* resource)
{
    char buf[1024];
    int len = get_line(client, buf, sizeof(buf));
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
        if (0 == len) { // '\r\n' has already been trimed out
            break;
        }
        char* begin = buf;
        char* end = buf;
        WALK_UNTIL(end, ':');
        if (0 == strncasecmp("Host", begin, end - begin)) {
            WALK(begin, end);   //skip ':'
            WALK(begin, end);
            request->host = dup_tok(begin, end);
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

static ResourceType get_type(char* suffix)
{
    if (0 == strncasecmp("HTML", suffix, 4))
        return RT_TEXT;
    else if (0 == strncasecmp("HTM", suffix, 3))
        return RT_TEXT;
    else if (0 == strncasecmp("TXT", suffix, 3))
        return RT_TEXT;
    else if (0 == strncasecmp("PHP", suffix, 3))
        return RT_PHP;
    else if (0 == strncasecmp("PY", suffix, 2))
        return RT_PY;
    else
        return RT_FILE;

}

static int fill_resource(Resource* resource, char* sbegin, char* send)
{
    if (sbegin == send) { //path not explicitly given
        sbegin = "/";
        send = sbegin + 1;
    }
    //TODO(wgtdkp): check if token(begin, end) too long
    resource->path_len = 
        tokcat(resource->path, resource->path_len, sbegin, send);

    char* begin = sbegin;
    char* end = sbegin;
    /*
     *skip the first '/'(there is always '/' at the beginning)
     */
    WALK_UNTIL(end, '/');
    begin = ++end;
    /*
     *check if request resource out of www_dir,
     *that is: the '../' is more than other entries(except './') until now.
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
         *is ok to cat '/' to path, even if path is ended by '/'
         */
        strcat(resource->path, "/");
        strcat(resource->path, default_file);
        resource->path_len += 1 + strlen(default_file);
        goto check;
    } else { //resource is a file
        end = &resource->path[resource->path_len - 1];
        while (*end != '.' && *end != '/') --end;
        if (*end == '/') {
            // TODO(wgtdkp): error: bad request
        }
        resource->type = get_type(end + 1);
    }
    if (resource->stat != RS_NOTFOUND)
        resource->size = st.st_size;

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
    for (i = 0; i < size; i++) {
        if (1 != recv(client, &buf[i], 1, 0) 
            || '\n' == buf[i]) {
            break;
        }
    }
    /*
     *trim out '\r\n'
     */
    if (buf[i] == '\n')
        buf[i] = 0;
    if (i >= 1 && buf[i - 1] == '\r')
        buf[--i] = 0;
    DEBUG(buf);
    return i;
}

static int handle_get(int client, Request* request, Resource* resource)
{
    int fd = -1;
    Response response;
    ResponseHeader* header = &response.header;
    assert(request->method == M_GET);
    if (is_static(resource->type)) {
        DEBUG("is static");
        // return static resource to client
        // TODOwgtdkp): support more types and subtypes
        switch (resource->type) {
        case RT_TEXT:
            header->content_type = MT_TEXT;
            header->content_subtype = MT_HTML;
            break;
        case RT_IMG:
            header->content_type = MT_IMG;
            header->content_subtype = MT_PNG;
            break;
        default: // response html page default
            header->content_type = MT_TEXT;
            header->content_subtype = MT_HTML;
            break;
        }

        switch(resource->stat) {
        case RS_OK:
            DEBUG("RS_OK"); 
            header->status = 200;
            header->content_length = resource->size;
            fd = open(resource->path, O_RDONLY, 00777);
            assert(fd != -1);
            response.content = (char*)mmap(NULL, 
                                        header->content_length, 
                                        PROT_READ, 
                                        MAP_SHARED, 
                                        fd, 
                                        0);
            break;
        case RS_NOTFOUND:
            DEBUG("RS_NOT_FOUND");
            header->status = 404;
            break;
        case RS_DENIED:
            DEBUG("RS_DENIED");
            header->status = 403;
            break;
        default:
            header->status = 404;
            break;
        }

        put_response(client, &response);
    } else {
        // TODO(wgtdkp): handle over to CGI
        assert(0);
    }

    // release content
    if (fd != -1) {
        close(fd);
        munmap(response.content, header->content_length);
    }
    return 0;
}

static int put_response(int client, Response* response)
{
    char begin[1024];
    char* buf = begin;
    ResponseHeader* header = &response->header;
    buf += sprintf(buf, "HTTP/%1d.%1d %3d %s \r\n", 
            http_version[0],
            http_version[1],
            header->status,
            status_repr(header->status));
    if (header->status == 200) {
        buf += sprintf(buf, "Contend-Type: %s/%s \r\n",
                content_type_repr(header->content_type),
                content_type_repr(header->content_subtype));
        buf += sprintf(buf, "Content-Length: %d \r\n",
                header->content_length);
    }
    buf += sprintf(buf, "\r\n");
    send(client, begin, buf - begin, 0);
    send(client, response->content, header->content_length, 0); 
    return 0;
}

static int handle_post(int client, Request* request, Resource* resource)
{
    assert(request->method == M_POST);
    return 0;
}

static void* handle_request(void* args)
{
    int client = *((int*)args);
    Request* request = create_request();
    Resource* resource = create_resource();
    parse_request_line(client, request, resource);
    parse_request_header(client, request);
    
    if (request->method == M_GET) {
        DEBUG("handle get...");
        handle_get(client, request, resource);
    } else if (request->method == M_POST) {
        handle_post(client, request, resource);
    } else {
        // TODO(wgtdkp): handle more request
    }
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

static const char* content_type_repr(MediaType type)
{
    switch(type) {
#       include "content_type.inc"
    }
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

static int config(void)
{
    strcpy(www_dir, "/home/wgtdkp/www");
    return 0;
}

int main(int args, char* argv[])
{
    // 0. arg parsing

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

    // 3. config
    config();

    // 4. listening loop
    printf("waiting for connection...\n");
    while (1) {
        client_sock = accept(server_sock,
            (struct sockaddr*)&client, &client_len);
        if (client_sock == -1) {
            //TODO(wgtdkp): error
            fprintf(stderr, "client connection failed\n");
        } else {
            //TODO(wgtdkp):
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
