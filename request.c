
#include "request.h"

#include "base/buffer.h"
#include "base/map.h"
#include "base/string.h"

#include "parse.h"
#include "response.h"
#include "server.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>


static int request_process_uri(request_t* request, response_t* response);
static int request_process_request_line(
        request_t* request, response_t* response);
static int request_process_headers(request_t* request, response_t* response);
static int request_process_body(request_t* request, response_t* response);

static int header_process_generic(
        request_t* request, int offset, response_t* response);
static int header_process_connection(
        request_t* request, int offset, response_t* response);
static int header_process_t_encoding(
        request_t* request, int offset, response_t* response);
static int header_process_host(
        request_t* request, int offset, response_t* response);

static int header_process_accept(
        request_t* request, int offset, response_t* response);
        


static int try_get_resource(request_t* request, response_t* response);


typedef struct {
    char* name;
    header_val_t val;
} header_nv_t;

#define HEADER_PAIR(name, processor)    \
    {#name, {offsetof(request_headers_t, name), processor}}
    
static header_nv_t header_tb[] = {
    HEADER_PAIR(cache_control, header_process_generic),
    HEADER_PAIR(connection, header_process_connection),
    HEADER_PAIR(date, header_process_generic),
    HEADER_PAIR(pragma, header_process_generic),
    HEADER_PAIR(trailer, header_process_generic),
    HEADER_PAIR(transfer_encoding, header_process_t_encoding),
    HEADER_PAIR(upgrade, header_process_generic),
    HEADER_PAIR(via, header_process_generic),
    HEADER_PAIR(warning, header_process_generic),
    HEADER_PAIR(allow, header_process_generic),
    HEADER_PAIR(content_encoding, header_process_generic),
    HEADER_PAIR(content_language, header_process_generic),
    HEADER_PAIR(content_length, header_process_generic),
    HEADER_PAIR(content_location, header_process_generic),
    HEADER_PAIR(content_md5, header_process_generic),
    HEADER_PAIR(content_range, header_process_generic),
    HEADER_PAIR(content_type, header_process_generic),
    HEADER_PAIR(expires, header_process_generic),
    HEADER_PAIR(last_modified, header_process_generic),
    
    HEADER_PAIR(accept, header_process_accept),
    HEADER_PAIR(accept_charset, header_process_generic),
    HEADER_PAIR(accept_encoding, header_process_generic),
    HEADER_PAIR(authorization, header_process_generic),
    HEADER_PAIR(expect, header_process_generic),
    HEADER_PAIR(from, header_process_generic),
    HEADER_PAIR(host, header_process_host),
    HEADER_PAIR(if_match, header_process_generic),
    HEADER_PAIR(if_modified_since, header_process_generic),
    HEADER_PAIR(if_none_match, header_process_generic),
    HEADER_PAIR(if_range, header_process_generic),
    HEADER_PAIR(if_unmodified_since, header_process_generic),
    HEADER_PAIR(max_forwards, header_process_generic),
    HEADER_PAIR(proxy_authorization, header_process_generic),
    HEADER_PAIR(range, header_process_generic),
    HEADER_PAIR(referer, header_process_generic),
    HEADER_PAIR(te, header_process_generic),
    HEADER_PAIR(user_agent, header_process_generic),
};

#undef HEADER_PAIR

#define HEADER_MAP_SIZE     (131)
static map_slot_t header_map_data[2 * HEADER_MAP_SIZE];
static map_t header_map = {
    .size = HEADER_MAP_SIZE,
    .max_size = 2 * HEADER_MAP_SIZE,
    .data = header_map_data,
    .cur = header_map_data + HEADER_MAP_SIZE
};


#define MIME_MAP_SIZE       (131)
static map_slot_t mime_map_data[2 * MIME_MAP_SIZE];

static map_t mime_map = {
  .size = MIME_MAP_SIZE,
  .max_size = 2 * MIME_MAP_SIZE,
  .data = mime_map_data,
  .cur = mime_map_data + MIME_MAP_SIZE  
};

static char* mime_tb [][2] = {
    {("htm"),     ("text/html")},
    {("html"),    ("text/html")},
    {("gif"),     ("image/gif")},
    {("ico"),     ("image/x-icon")},
    {("jpeg"),    ("image/jpeg")},
    {("jpg"),     ("image/jpeg")},
    {("svg"),     ("image/svg+xml")},
    {("txt"),     ("text/plain")},
    {("zip"),     ("application/zip")},  
};


void header_map_init(void)
{
    int n = sizeof(header_tb) / sizeof(header_tb[0]);
    for (int i = 0; i < n; i++) {
        string_t key = string_setto(header_tb[i].name);
        map_val_t val;
        val.header = header_tb[i].val;
        map_put(&header_map, key, val);
    }
}

void mime_map_init(void)
{
    int n = sizeof(mime_tb) / sizeof(mime_tb[0]);
    for (int i = 0; i < n; i++) {
        string_t key = string_setto(mime_tb[i][0]);
        map_val_t val;
        val.mime = string_setto(mime_tb[i][1]);
        map_put(&mime_map, key, val);
    }    
}

static int header_process_generic(
        request_t* request, int offset, response_t* response)
{   
    string_t* member = (string_t*)((void*)&request->headers + offset);
    *member = request->header_value;
    return 0;
}

/*
 * Request
 */

void request_init(request_t* request)
{
    request->method = M_GET;    // Any value is ok
    request->version.major = 0;
    request->version.minor = 0;

    memset(&request->headers, 0, sizeof(request->headers));
    
    list_init(&request->accepts, &accept_pool);
    
    string_init(&request->request_line);
    string_init(&request->header_name);
    string_init(&request->header_value);
    memset(&request->uri, 0, sizeof(request->uri));
    
    request->stage = RS_REQUEST_LINE;
    request->state = 0; // RL_S_BEGIN
    request->uri_state = 0; // Meaningless
    request->keep_alive = false;
    request->t_encoding = TE_IDENTITY;
    request->content_length = -1;
    
    buffer_init(&request->buffer);
}

void request_clear(request_t* request)
{
    int tmp = request->keep_alive;
    request_init(request);
    request->keep_alive = tmp;
}

void request_release(request_t* request)
{
    // TODO(wgtdkp): release dynamic allocated resource
    // TODO(wgtdkp): ther won't by any dynamic allocated resource!!!
}

int handle_request(connection_t* connection)
{
    int err = OK;
    request_t* request = &connection->request;
    buffer_t* buffer = &request->buffer;
    queue_node_t* response_node = queue_alloc(&connection->response_queue);
    response_t* response = (response_t*)&response_node->data;
    //response_t* response = pool_alloc(&connection->response_pool);
    response_init(response);
    
    int readed = buffer_recv(buffer, connection->fd);
    // Client closed the connection
    if (readed <= 0) {
        readed = -readed;
        //printf("connection closed by the client side\n");
        //fflush(stdout);
        close_connection(connection);
        return OK;
    }

    //if (buffer_size(buffer) > 0)
    //    print_string("%*s", (string_t){buffer->begin, buffer->end});

    if (err == OK && request->stage == RS_REQUEST_LINE)
        err = request_process_request_line(request, response);
    
    if (err == OK && request->stage == RS_HEADERS)
        err = request_process_headers(request, response);

    if (err == OK && request->stage == RS_BODY)
        err = request_process_body(request, response);
    
    if (err == AGAIN)
        return err;
    else if (err == OK) {
        
        try_get_resource(request, response);
        response_build(response, request);
    }
    // TODO(wgtdkp): request done, 
    request_clear(request);
    
    queue_push(&connection->response_queue, response_node);
    
    // Send response(s) directly, until send buffer is full.
    err = handle_response(connection);
    
    return err;
}

static int request_process_uri(request_t* request, response_t* response)
{
    uri_t* uri = &request->uri;
    *uri->abs_path.end = 0;  // It is safe to do this
    const char* rel_path;
    if (uri->abs_path.end - uri->abs_path.begin == 1) 
        rel_path = "./";
    else
        rel_path = uri->abs_path.begin + 1;
    int fd = openat(doc_root_fd, rel_path, O_RDONLY);
    
    // Open the requested resource failed
    if (fd == -1) {
        response_build_err(response, request, 404);
        return ERR_STATUS(response->status);
    }
    
    struct stat* stat = &response->resource_stat;
    fstat(fd, stat);
    if (S_ISDIR(stat->st_mode)) {
        fd = openat(fd, "index.html", O_RDONLY);
        if (fd == -1) {
            response_build_err(response, request, 404);
            return ERR_STATUS(response->status);
        }
        fstat(fd, &response->resource_stat);
        uri->extension = STRING("html");
    }
    
    response->resource_fd = fd;
    return OK;
}

static int request_process_request_line(
        request_t* request,
        response_t* response)
{
    int err = parse_request_line(request);
    if (err == AGAIN) {
        return err;
    } else if (err != OK) { // Reuqest line parsing error, can't recovery
        response->must_close = true;
        response_build_err(response, request, 400);
        return err;
    }
    request->stage = RS_HEADERS;
    
    // Supports only HTTP/1.1 and HTTP/1.0
    if (request->version.major != 1 || request->version.minor > 2) {
        response->must_close = true;
        response_build_err(response, request, 505);
        return ERR_STATUS(response->status);
    }
    
    // HTTP/1.1: persistent connection default
    if (request->version.minor == 1)
        request->keep_alive = 1;
    else
        request->keep_alive = 0;

    // TODO(wgtdkp): check method
    
    // We still need to receive the left part of this request
    // Thus, the connection will hold
    request_process_uri(request, response);
    return OK;
}

static int request_process_headers(request_t* request, response_t* response)
{
    request_headers_t* headers = &request->headers;
    while (true) {
        int err = parse_header_line(request);
        switch (err) {
        case AGAIN:
            return AGAIN;
        case EMPTY_LINE:
            goto done;
        case OK:
            {
                map_slot_t* slot = map_get(&header_map, request->header_name);
                if (slot == NULL)
                    break;
                header_val_t header = slot->val.header;
                if (header.offset != -1) {
                    int err = header.processor(request,
                            header.offset, response);
                    if (err != 0)
                        return err; 
                }
            }
            break; 
        default:
            assert(0);
        }
    }
done:
    request->stage = RS_BODY;
    
    /* HTTP version */
    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html
    // [14.23] Host
    if (request->version.minor == 1) {
        if (headers->host.begin == NULL) {
            response_build_err(response, request, 400);
            return ERR_STATUS(response->status);
        }
    }
    
    /* Pragma */
    
    
    // TODO(wgtdkp): process content-length if needed
    return OK;
}

static int header_process_connection(
        request_t* request, int offset, response_t* response)
{
    header_process_generic(request, offset, response);
    request_headers_t* headers = &request->headers;
    if(strncasecmp("close", headers->connection.begin, 5) == 0)
        request->keep_alive = 0;
    
    return OK;
}

static int header_process_t_encoding(
        request_t* request, int offset, response_t* response)
{
    header_process_generic(request, offset, response);
    string_t* transfer_encoding = &request->headers.transfer_encoding;
    if (strncasecmp("chunked", transfer_encoding->begin, 7) == 0) {
        request->t_encoding = TE_CHUNKED;
    } else if (strncasecmp("gzip", transfer_encoding->begin, 4) == 0
            || strncasecmp("x-gzip", transfer_encoding->begin, 6) == 0) {
        request->t_encoding = TE_GZIP;            
    } else if (strncasecmp("compress", transfer_encoding->begin, 8) == 0
            || strncasecmp("x-compress", transfer_encoding->begin, 10) == 0) {
        request->t_encoding = TE_COMPRESS;            
    } else if (strncasecmp("deflate", transfer_encoding->begin, 7) == 0) {
        request->t_encoding = TE_DEFLATE;   
    } else if (strncasecmp("identity", transfer_encoding->begin, 8) == 0) {
        request->t_encoding = TE_IDENTITY;
    } else {
        // Must close the connection as we can't understand the body
        response->must_close = true;
        response_build_err(response, request, 415);
        return ERR_STATUS(response->status);
    }
    
    return OK;
}

static int header_process_host(
        request_t* request, int offset, response_t* response)
{
    header_process_generic(request, offset, response);
    return OK;
}

static int request_process_body(request_t* request, response_t* response)
{
    buffer_t* buffer = &request->buffer;
    if (buffer_size(buffer) == 0)
        return OK;
    int err = OK;
    
    if (request->t_encoding == TE_IDENTITY) {
        if (request->content_length < 0) {
            response_build_err(response, request, 400);
            return ERR_STATUS(response->status);
        }
        err = parse_request_body_no_encoding(request);
            
    } else if (request->t_encoding == TE_CHUNKED)
        err = parse_request_body_chunked(request);
    
    switch (err) {
    case AGAIN:
        return AGAIN;
    case OK:
        break;
    default:
        response_build_err(response, request, 400);
        return ERR_STATUS(response->status);
    }
    
    // Parse body done
    request->stage = RS_REQUEST_LINE;
    

    return OK;
}

static int header_process_accept(
        request_t* request, int offset, response_t* response)
{
    header_process_generic(request, offset, response);
    
    parse_accept_value(request);
    
    return OK;
}

static int try_get_resource(request_t* request, response_t* response)
{
    return OK;
}
