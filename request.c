#include "server.h"

static int request_handle_uri(request_t* request, response_t* response);
static int request_handle_request_line(
        request_t* request, response_t* response);
static int request_handle_headers(request_t* request, response_t* response);
static int request_handle_body(request_t* request, response_t* response);

static int header_handle_generic(
        request_t* request, int offset, response_t* response);
static int header_handle_connection(
        request_t* request, int offset, response_t* response);
static int header_handle_t_encoding(
        request_t* request, int offset, response_t* response);
static int header_handle_content_length(
        request_t* request, int offset, response_t* response);
static int header_handle_host(
        request_t* request, int offset, response_t* response);
static int header_handle_accept(
        request_t* request, int offset, response_t* response);
static int header_handle_if_modified_since(
        request_t* request, int offset, response_t* response);

static int request_process_headers(request_t* request, response_t* response);

typedef struct {
    string_t name;
    header_val_t val;
} header_nv_t;

#define HEADER_PAIR(name, processor)    \
    {STRING(#name), {offsetof(request_headers_t, name), processor}}
    
static header_nv_t header_tb[] = {
    HEADER_PAIR(cache_control, header_handle_generic),
    HEADER_PAIR(connection, header_handle_connection),
    HEADER_PAIR(date, header_handle_generic),
    HEADER_PAIR(pragma, header_handle_generic),
    HEADER_PAIR(trailer, header_handle_generic),
    HEADER_PAIR(transfer_encoding, header_handle_t_encoding),
    HEADER_PAIR(upgrade, header_handle_generic),
    HEADER_PAIR(via, header_handle_generic),
    HEADER_PAIR(warning, header_handle_generic),
    HEADER_PAIR(allow, header_handle_generic),
    HEADER_PAIR(content_encoding, header_handle_generic),
    HEADER_PAIR(content_language, header_handle_generic),
    HEADER_PAIR(content_length, header_handle_content_length),
    HEADER_PAIR(content_location, header_handle_generic),
    HEADER_PAIR(content_md5, header_handle_generic),
    HEADER_PAIR(content_range, header_handle_generic),
    HEADER_PAIR(content_type, header_handle_generic),
    HEADER_PAIR(expires, header_handle_generic),
    HEADER_PAIR(last_modified, header_handle_generic),
    
    HEADER_PAIR(accept, header_handle_accept),
    HEADER_PAIR(accept_charset, header_handle_generic),
    HEADER_PAIR(accept_encoding, header_handle_generic),
    HEADER_PAIR(authorization, header_handle_generic),
    HEADER_PAIR(cookie, header_handle_generic),
    HEADER_PAIR(expect, header_handle_generic),
    HEADER_PAIR(from, header_handle_generic),
    HEADER_PAIR(host, header_handle_host),
    HEADER_PAIR(if_match, header_handle_generic),
    HEADER_PAIR(if_modified_since, header_handle_if_modified_since),
    HEADER_PAIR(if_none_match, header_handle_generic),
    HEADER_PAIR(if_range, header_handle_generic),
    HEADER_PAIR(if_unmodified_since, header_handle_generic),
    HEADER_PAIR(max_forwards, header_handle_generic),
    HEADER_PAIR(proxy_authorization, header_handle_generic),
    HEADER_PAIR(range, header_handle_generic),
    HEADER_PAIR(referer, header_handle_generic),
    HEADER_PAIR(te, header_handle_generic),
    HEADER_PAIR(user_agent, header_handle_generic),
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

void header_map_init(void)
{
    int n = sizeof(header_tb) / sizeof(header_tb[0]);
    for (int i = 0; i < n; ++i) {
        map_val_t val;
        val.header = header_tb[i].val;
        map_put(&header_map, &header_tb[i].name, &val);
    }
}

static inline void uri_init(uri_t* uri)
{
    memset(uri, 0, sizeof(uri_t));
    uri->state = URI_S_BEGIN;
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
    
    uri_init(&request->uri);
    
    string_init(&request->host);
    request->port = 80;
    
    request->stage = RS_REQUEST_LINE;
    request->state = RL_S_BEGIN; 
    request->keep_alive = false;
    request->t_encoding = TE_IDENTITY;
    request->content_length = -1;
    
    buffer_init(&request->buffer);
    
    request->response = NULL;
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
    
    if (request->response == NULL) {
        request->response = queue_alloc(&connection->response_queue);
        response_init(request->response);
    }
    
    int readed = buffer_recv(buffer, connection->fd);
    // Client closed the connection
    if (readed <= 0) {
        readed = -readed;
        close_connection(connection);
        return OK;
    }
    // Print request
    if (buffer_size(buffer) > 0)
        print_buffer(buffer);
        //print_string("%*s", &(string_t){buffer->begin, buffer_size(buffer)});

    if (err == OK && request->stage == RS_REQUEST_LINE) {
        err = request_handle_request_line(request, request->response);
    }
    
    if (err == OK && request->stage == RS_HEADERS) {
        err = request_handle_headers(request, request->response);
    }
    
    // TODO(wgtdkp): handle Expect: 100 continue before parse body
    if (err == OK && request->stage == RS_BODY) {
        err = request_handle_body(request, request->response);
        if (err != AGAIN) {
            response_build(request->response, request);
        }
    }
    
    if (err == AGAIN)
        return err;
    
    queue_push(&connection->response_queue, request->response);
    request_clear(request);
    
    // Send response(s) directly, until send buffer is full.
    err = handle_response(connection);
    
    return err;
}

static location_t* match_location(string_t* path)
{
    vector_t* locs = &server_cfg.locations;
    for (int i = 0; i < locs->size; ++i) {
        location_t* loc = vector_at(locs, i);
        // The first matched location is returned
        if (strncasecmp(loc->path.data, path->data, loc->path.len) == 0)
            return loc;
    }
    //assert(false);
    return NULL;
}

static int request_handle_uri(request_t* request, response_t* response)
{
    uri_t* uri = &request->uri;
    if (uri->host.data) {
        request->host = uri->host;
        if (uri->port.data)
            request->port = atoi(uri->port.data);
    }
    uri->abs_path.data[uri->abs_path.len] = 0;  // It is safe to do this
    request->loc = match_location(&uri->abs_path);
    if (request->loc == NULL) {
        response_build_err(response, request, 404);
        return ERR_STATUS(response->status);
    }
    if (request->loc->pass)
        return OK;
    
    const char* rel_path;
    if (uri->abs_path.len == 1) { 
        rel_path = "./";
    } else {
        rel_path = uri->abs_path.data + 1;
    }

    int fd = openat(root_fd, rel_path, O_RDONLY);
    
    // Open the requested resource failed
    if (fd == -1) {
        response_build_err(response, request, 404);
        return ERR_STATUS(response->status);
    }
    
    struct stat* stat = &response->resource_stat;
    fstat(fd, stat);
    if (S_ISDIR(stat->st_mode)) {
        int tmp_fd = fd;
        fd = openat(fd, "index.html", O_RDONLY);
        close(tmp_fd); // FUCK ME !!!
        if (fd == -1) {
            // Accessing to a directory is forbidden
            response_build_err(response, request, 403);
            return ERR_STATUS(response->status);
        }
        fstat(fd, &response->resource_stat);
        uri->extension = STRING("html");
    }

    response->resource_fd = fd;
    return OK;
}

static int request_handle_request_line(
        request_t* request,
        response_t* response)
{
    int err = parse_request_line(request);
    if (err == AGAIN) {
        return err;
    } else if (err != OK) { // Reuqest line parsing error, can't recovery
        response->keep_alive = false;
        response_build_err(response, request, 400);
        return err;
    }
    request->stage = RS_HEADERS;
    
    // Supports only HTTP/1.1 and HTTP/1.0
    if (request->version.major != 1 || request->version.minor > 2) {
        response->keep_alive = false;
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
    return request_handle_uri(request, response);
}

static int request_handle_headers(request_t* request, response_t* response)
{
    int err;
    while (true) {
        err = parse_header_line(request);
        switch (err) {
        case AGAIN:
            return AGAIN;
        case EMPTY_LINE:
            goto done;
        case OK: {
            if (string_eq(&request->header_name, &STRING("content_length"))) {
                printf("error");
            }
            map_slot_t* slot = map_get(&header_map, &request->header_name);
            if (slot == NULL)
                break;
            header_val_t header = slot->val.header;
            if (header.offset != -1) {
                header_processor_t processor = header.processor;
                int err = processor(request, header.offset, response);
                if (err != 0)
                    return err;
            }
        } break;
        default:
            assert(0);
        }
    }
    
done:
    err = request_process_headers(request, response);
    request->stage = RS_BODY;
    return err;
}

static int header_handle_connection(
        request_t* request, int offset, response_t* response)
{
    header_handle_generic(request, offset, response);
    request_headers_t* headers = &request->headers;
    if(strncasecmp("close", headers->connection.data, 5) == 0)
        request->keep_alive = 0;
    return OK;
}

static int header_handle_t_encoding(
        request_t* request, int offset, response_t* response)
{
    header_handle_generic(request, offset, response);
    
    string_t* transfer_encoding = &request->headers.transfer_encoding;
    if (strncasecmp("chunked", transfer_encoding->data, 7) == 0) {
        request->t_encoding = TE_CHUNKED;
    } else if (strncasecmp("gzip", transfer_encoding->data, 4) == 0
            || strncasecmp("x-gzip", transfer_encoding->data, 6) == 0) {
        request->t_encoding = TE_GZIP;            
    } else if (strncasecmp("compress", transfer_encoding->data, 8) == 0
            || strncasecmp("x-compress", transfer_encoding->data, 10) == 0) {
        request->t_encoding = TE_COMPRESS;            
    } else if (strncasecmp("deflate", transfer_encoding->data, 7) == 0) {
        request->t_encoding = TE_DEFLATE;   
    } else if (strncasecmp("identity", transfer_encoding->data, 8) == 0) {
        request->t_encoding = TE_IDENTITY;
    } else {
        // Must close the connection as we can't understand the body
        response->keep_alive = false;
        response_build_err(response, request, 415);
        return ERR_STATUS(response->status);
    }
    
    return OK;
}

static int header_handle_content_length(
        request_t* request, int offset, response_t* response)
{
    assert(string_eq(&request->header_name, &STRING("content_length")));
    header_handle_generic(request, offset, response);
    //sring_t* name = &request->header_name;
    string_t* val = &request->header_value;
    // Header values are always terminated by '\r\n' or '\n'
    // It means setting val->data[val->len] = 0 is safe
    val->data[val->len] = 0;
    int len = atoi(val->data);
    if (len < 0) {
        response_build_err(response, request, 400);
        return ERR_STATUS(response->status);
    }
    if (request->method == M_GET || request->method == M_HEAD) {
        request->discard_body = 1;
    }
    request->content_length = len;
    return OK;
}

// If both the uri in the request contains host[:port]
// and has this host header, the host header is active.
static int header_handle_host(
        request_t* request, int offset, response_t* response)
{
    header_handle_generic(request, offset, response);

    return OK;
}

static int header_handle_if_modified_since(
        request_t* request, int offset, response_t* response)
{
    header_handle_generic(request, offset, response);

    return OK;
}


static int request_handle_body(request_t* request, response_t* response)
{
    int err = OK;
    switch (request->t_encoding) {
    case TE_IDENTITY:
        err = parse_request_body_identity(request);
        break;
    case TE_CHUNKED:
        err = parse_request_body_chunked(request);
        break;
    default:
        // TODO(wgtdkp): cannot understanding
        // May discard the body
        ;
    }
       
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

static int header_handle_accept(
        request_t* request, int offset, response_t* response)
{
    header_handle_generic(request, offset, response);
    
    //parse_header_accept(request);
    return OK;
}

static int header_handle_generic(
        request_t* request, int offset, response_t* response)
{
    string_t* member = (string_t*)((char*)&request->headers + offset);
    *member = request->header_value;
    return OK;
}

int request_process_headers(request_t* request, response_t* response)
{
    request_headers_t* headers = &request->headers;

    // RFC 2616 [14.23] Host
    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html
    if (headers->host.data) {
        parse_header_host(request);
    }
    
    // HTTP/1.1 must has the 'Host'' header
    if ((request->version.minor == 1 && headers->host.data == NULL)
        || request->host.data == NULL) {
        response_build_err(response, request, 400);
        return ERR_STATUS(response->status);
    }
    
    if (headers->cache_control.data) {
        // Nothing to do
        
    }
    
    if (headers->expect.data) {
        if (strncasecmp("100-continue", headers->expect.data,
                sizeof("100-continue") - 1)) {
            // Have not received entity data
            // And we need the entity
            if (buffer_size(&request->buffer) == 0
                    && !request->discard_body) {
                response->status = 100;
            } else {
                response_build_err(response, request, 417);
                return ERR_STATUS(response->status);
            }
        }
    }
    
    // If error, log this email to contact
    if (headers->from.data) {
        // Used for logger
    }
    
    if (headers->if_modified_since.data) {
        struct tm tm;
        if (strptime(headers->if_modified_since.data,
                "%a,%n%d%n%b%n%Y%n%H:%M:%S%nGMT", &tm) != NULL) {
            time_t tm_sec = mktime(&tm);
            if (tm_sec > response->resource_stat.st_mtime) {
                // RFC 2616 [14.25]
                if (response->status != 200)
                    response->status = 304;
            }
            
            // TODO(wgtdkp): handle 'range' header
        }
    }
    
    // undefied, if also contains 'If-Modified-Since' header
    if (headers->if_unmodified_since.data) {
        struct tm tm;
        if (strptime(headers->if_unmodified_since.data,
                "%a,%n%d%n%b%n%Y%n%H:%M:%S%nGMT", &tm) != NULL) {
            time_t tm_sec = mktime(&tm);
            if (tm_sec <= response->resource_stat.st_mtime) {
                // RFC 2616 [14.28]
                if (response->status / 100 == 2) {
                    response_build_err(response, request, 417);
                    return ERR_STATUS(response->status);
                }
            }
        }
    }
    
    // TODO(wgtdkp): handle 'range' header
    //if (headers->range.data) {
    //    
    //}

    // TODO(wgtdkp): handle 'referer' header
    //if (headers->referer.data) {
    //    
    //}
    

    return OK;
}
