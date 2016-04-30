#include "request.h"
#include "buffer.h"
#include "parse.h"
#include "string.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WALK(begin, end)    while (*end != 0 && isspace((int)(*end))) { ++end; } \
                            begin = end;  \
                            while (*end != 0 && !isspace((int)(*end))) { ++end; }

#define WALK_UNTIL(begin, ch)   while (*begin != 0 && *begin != ch) ++begin;

#define IS_DOT(begin, end)  ((end) - (begin) == 1 && *(begin) == '.')
#define IS_DDOT(begin, end) ((end) - (begin) == 2 && *(begin) == '.' && *((begin) + 1) == '.')


static int dump_request(request_t* request)
{
    printf("status: %d\n", request->status);
    printf("method: %d\n", request->method);
    printf("version: %d.%d\n", request->version.major, request->version.minor);
    print_string("request_line: %*s\n", request->request_line_begin,
            request->request_line_end - request->request_line_begin);
    print_string("uri: %*s\n", request->uri_begin,
            request->uri_end - request->uri_begin);
    print_string("schema: %*s\n", request->schema_begin,
            request->schema_end - request->schema_begin);
    print_string("host: %*s\n", request->host_begin,
            request->host_end - request->host_begin);
    printf("state: %d\n", request->state);
    printf("reuqest_line_done: %d\n", request->request_line_done);
    printf("keep_alive: %d\n", request->keep_alive);
    printf("saw_eof: %d\n", request->saw_eof);
    printf("buffer size: %d\n", buffer_size(&request->buffer));
    fflush(stdout);
    return 0;
}

/*
static const char* method_repr(Method method);
static int get_line(int client, char* buf, int size);
static int get_method(char* str, int len);
*/

/****** request *******/
void request_init(request_t* request)
{
    request->status = 400;
    request->method = M_GET;    // Any value is ok
    request->version.major = 0;
    request->version.minor = 0;

    request->request_line_begin = NULL;
    request->request_line_end = NULL;
    request->method_begin = NULL;
    request->uri_begin = NULL;
    request->uri_end = NULL;
    request->schema_begin = NULL;
    request->schema_end = NULL;
    request->host_begin = NULL;
    request->host_end = NULL;

    request->state = 0;
    request->request_line_done = false;
    request->keep_alive = false;
    request->saw_eof = false;

    buffer_init(&request->buffer);
}

void request_release(request_t* request)
{
    // TODO(wgtdkp): release dynamic allocated resource
}

// Do not free dynamic allocated memory
void request_clear(request_t* request)
{
    request_init(request);
    //string_clear(&request->query_string);
    //string_clear(&request->path);
}

int handle_request(connection_t* connection)
{
    request_t* request = &connection->request;
    buffer_t* buffer = &request->buffer;
    int readed = buffer_read(connection->fd, buffer);
    
    // Client closed the connection
    if (readed <= 0) {
        readed = -readed;
        request->saw_eof = true;
        // TODO(wgtdkp): remove EPOLLINT events
    }

    if (buffer_size(buffer) > 0)
        print_string("recv data: %*s\n", buffer->begin, buffer_size(buffer));
    
    int err = request_parse(request);
    if (err != OK) {
        // TODO(wgtdkp): update request state
        // Response bad request
        printf("err: %d\n", err);
        fflush(stdout);
        connection_close(connection);
        dump_request(request);
        //assert(0);
    }

    // TOD(wgtdkp): process headers

    if (buffer->end >= buffer->limit) {
        // TODO(wgtdkp): too bigger request header
        assert(0);
    }

    if (request->request_line_done) {
        printf("parse request line done\n");
        fflush(stdout);
    }

    // DEBUG: 
    if (request->saw_eof) {
        printf("connection closed\n");
        fflush(stdout);
        connection_close(connection);
        dump_request(request);
    }

    return 0;

    // TODO(wgtdkp): check if request is all ready(completely parsed)

    // TODO(wgtdkp): perform request

    // TODO(wgtdkp): build and send response


    //int fd = connection->fd;
    //char buffer[1024];
    //read(fd, buffer, 1024);
    //send_test_response(connection);

    //event_set_to(connection, EVENTS_OUT);
/*
    //int client = *((int*)args);
    while (true) {
        bool keep_alive = false;
        //buffer_t* request_buf = create_buffer(100);
        request_t* request = create_request();
        Resource* resource = create_resource();
        if (0 != parse_request_line(client, request, resource))
            goto close;
        if (0 != parse_request_header(client, request))
            goto close;

        keep_alive = request->keep_alive;
        //ju_log("%s %s \n", method_repr(request->method), resource->path);

        response_t response = default_response;

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
}


/****** lexer ******/


/*
//return error
static int parse_request_line(int client, request_t* request)
{
    char buf[1024];
    int len = get_line(client, buf, sizeof(buf));
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

    //parse_uri(begin, end, request);
    WALK(begin, end);
    if (0 == strncasecmp("HTTP/1.1", begin, end - begin)) {
        request->version[0] = request->version[1] = 1;
    } else if (0 == strncasecmp("HTTP/1.1", begin, end - begin)) {
        request->version[0] = 1;
        request->version[1] = 0;
    }
    return 0;
}
*/

/*
//return: -1, error;
static int parse_request_header(int client, request_t* request)
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
*/

/*
static int fill_resource(Resource* resource, char* sbegin, char* send)
{
    if (sbegin == send) { // path not explicitly given
        sbegin = "/";
        send = sbegin + 1;
    }
    // TODO(wgtdkp): use string_t
    //resource->path_len = 
    //    tokcat(resource->path, resource->path_len, sbegin, send);


    char* begin = sbegin;
    char* end = sbegin;
    
    // skip the first '/'(there is always '/' at the beginning)
    WALK_UNTIL(end, '/');
    begin = ++end;
    
    // Check if request resource out of www_dir,
    // that is: the '../' is more than other entries(except './') until now.
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
        // It's ok to cat '/' to path, 
        // even if path is ended by '/'
        strcat(resource->path, "/");
        strcat(resource->path, default_index);
        resource->path_len += 1 + strlen(default_index);
        goto check;
    } else {} //resource is a file

    return 0;
}
*/

/*
//return: -1, error;
static int parse_uri(char* sbegin, char* send, request_t* request)
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
    
    // TODO(wgtdkp): get resource info
    //fill_resource(resource, begin, end);


    begin = end;
    WALK(begin, end);
    if (begin != end)
        string_append(&request->query_string, begin + 1, end - (begin + 1));

    *send = tmp;
    return 0;
}
*/

/*
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
    // Trim '\r\n'
    if (buf[i] == '\n')
        buf[i] = 0;
    if (i >= 1 && buf[i - 1] == '\r')
        buf[--i] = 0;
    //DEBUG(buf);
    return i;
}
*/

/*
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
*/

/*
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
*/

/*
static inline int empty_line(char* line, int len)
{
    return (len >= 2 && line[0] == '\r' && line[1] == '\n') \
        || (len == 1 && line[0] == '\n');
}
*/

/*
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
*/
