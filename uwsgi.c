#include "server.h"

static int uwsgi_start_request(int fd, connection_t* connection);

/*
 * Takeover request from server
 */
int uwsgi_takeover(connection_t* connection)
{
    request_t* request = &connection->request;
    response_t* response = request->response;
    if (request->pass) {
        int err = uwsgi_start_request(response->resource_fd, connection);
        request->pass = false;
        if (err != OK)
            return err;
    }
    // Shouldn't try to receive until the event triggered
    //uwsgi_fetch_response(response);
    //printf("%d\n", buffer_size(&response->buffer)); fflush(stdout);
    return OK;
}

static int uwsgi_buffer_append_kv(buffer_t* buffer, string_t* k, string_t* v)
{
    int len = buffer_append_u16le(buffer, k->len);
    len += buffer_append_string(buffer, k);
    len += buffer_append_u16le(buffer, v->len);
    len += buffer_append_string(buffer, v);
    return len;
}

#define UWSGI_KV_LEN(k, v)  (sizeof(k) - 1 + sizeof(v) - 1 + 2 + 2)
#define UWSGI_KSV_LEN(k, v) (sizeof(k) -1 + (v)->len + 2 + 2)

static int uwsgi_start_request(int fd, connection_t* connection)
{
    request_t* request = &connection->request;
    response_t* response = request->response;
    assert(request->pass);
    uri_t* uri = &request->uri;
    request_headers_t* headers = &request->headers;
    string_t script_name = string_null;
    string_t path_info = string_null;
    script_name = STRING("");
    path_info = uri->abs_path;

    buffer_t buf;
    buffer_init(&buf);
    buffer_append_u32le(&buf, 0);
    // Environments
    if (request->method == M_GET) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("GET"));
    } else if (request->method == M_POST) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("POST"));
    } else {
        return response_build_err(response, request, 405);
    }
    uwsgi_buffer_append_kv(&buf, &STRING("SCRIPT_NAME"), &script_name);
    uwsgi_buffer_append_kv(&buf, &STRING("PATH_INFO"), &path_info);
    uwsgi_buffer_append_kv(&buf, &STRING("QUERY_STRING"), &uri->query);
    uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_TYPE"), &headers->content_type);
    uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_LENGTH"), &headers->content_length);
    uwsgi_buffer_append_kv(&buf, &STRING("HTTP_HOST"), &headers->host);
    if (headers->cookie.len)
        uwsgi_buffer_append_kv(&buf, &STRING("HTTP_COOKIE"), &headers->cookie);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_NAME"), &uri->host);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PORT"), &uri->port);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PROTOCOL"), &STRING("HTTP/1.1"));

    char* p = buf.begin + 1;
    uint16_t data_size = buffer_size(&buf) - 4;
    *p++ = data_size & 0xff;
    *p++ = (data_size >> 8) & 0xff;
    
    // Blocking send
    // As the request from the client is buffered by julia,
    // blocking send should be fine.
    buffer_send(&buf, fd);
    // Send the body
    assert(buffer_size(&request->buffer) == request->body_received);
    buffer_send(&request->buffer, fd);
    
    set_nonblocking(fd);
    back_connection_t* back_connection = pool_alloc(&back_connection_pool);
    back_connection->fd = fd;
    back_connection->side = C_SIDE_BACK;
    back_connection->front = connection;
    julia_epoll_event_t event = {
        .events = EVENTS_IN,
        .data.ptr = back_connection
    };
    // Register event in
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        return response_build_err(response, request, 404);
    }
    return OK;
}

int uwsgi_abort_request()
{
    return OK;
}

int uwsgi_fetch_response(response_t* response)
{
    assert(response->fetch_from_back);
    if (response->resource_fd == -1)
        return OK; // Have already fetched all data
    buffer_t* buffer = &response->buffer;
    int read_n = buffer_recv(buffer, response->resource_fd);
    //print_buffer(buffer);    
    if (read_n <= 0) { // We done fetch the whole response from backend
        read_n = -read_n;
        // The connection to backend have already been closed by uwsgi server
        close(response->resource_fd); // The registered events auto removed
        response->resource_fd = -1;
        pool_free(&back_connection_pool, response->back_connection);
        response->back_connection = NULL;
        return OK;
    }
    return AGAIN; // There are still data from uwsgi
}

int uwsgi_open_connection(location_t* loc)
{
    assert(loc->pass);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON(fd == -1, "socket");

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(loc->port);
    int success = inet_pton(AF_INET, loc->host.data, &addr.sin_addr);
    if (success <= 0)
        return ERROR;
    
    int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err < 0)
        return ERROR;
    return fd;
}
