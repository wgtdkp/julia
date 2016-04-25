#include "request.h"
#include "util.h"

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


/*
static const char* method_repr(Method method);
static int get_line(int client, char* buf, int size);
static int get_method(char* str, int len);
*/

/****** request *******/
void request_init(Request* request)
{
    request->method = M_GET;
    request->version[0] = 1;
    request->version[1] = 1;
    string_init(&request->query_string, 0);
    string_init(&request->path, 0);
    request->keep_alive = true;
    request->ready = false;
}

void request_release(Request* request)
{
    string_release(&request->query_string);
    string_release(&request->path);
}

// Do not free dynamic allocated memory
void request_clear(Request* request)
{
    request_init(request);
    //string_clear(&request->query_string);
    //string_clear(&request->path);
}

int request_parse(Request* request)
{
    return 0;
}

// Read data to buffer
// Return: bytes readed; 0, indicates end of file;
int request_read(int fd, Request* request)
{
    Buffer* buffer = request->buffer;
    int readed = 0;
    do {
        int margin = buffer->capacity - buffer->size;
        int len = read(fd, buffer->data + buffer->size, margin);
        if (len == 0)   // EOF
            return 0;
        if (len == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            EXIT_ON(true, "read");  // Unhandled error
        }
        readed += len;
        buffer->size += len;
    } while (buffer->size < buffer->capacity);  // We may have not read all data
    return readed;
}

bool request_need_parse(Request* request, int last_buffer_size)
{
    Buffer* buffer = request->buffer;
    // TODO(wgtdkp): what about waiting for body?
    // Detect end of request header
    const char* end_of_header = "\r\n\r\n";
    const int val = *((int*)end_of_header);
    // We restart detecting end of header from the last position we stoped
    char* begin = buffer->data + MAX(last_buffer_size - 4, 0);
    char* end = buffer->data + buffer->size - 3;
    // KMP is not really needed here
    for (char* p = begin; p < end; p++) {
        if (val == *((int*)p))
            return true;
    }

    
    return false;
}

/****** lexer ******/


/*
//return error
static int parse_request_line(int client, Request* request)
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
*/

/*
static int fill_resource(Resource* resource, char* sbegin, char* send)
{
    if (sbegin == send) { // path not explicitly given
        sbegin = "/";
        send = sbegin + 1;
    }
    // TODO(wgtdkp): use String
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
static int parse_uri(char* sbegin, char* send, Request* request)
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
