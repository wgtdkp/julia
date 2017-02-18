#include "server.h"

/*
static int uwsgi_buffer_append_kv(buffer_t* buffer, string_t* k, string_t* v) {
    int len = buffer_append_u16le(buffer, k->len);
    len += buffer_append_string(buffer, k);
    len += buffer_append_u16le(buffer, v->len);
    len += buffer_append_string(buffer, v);
    return len;
}
*/
#define UWSGI_KV_LEN(k, v)  (sizeof(k) - 1 + sizeof(v) - 1 + 2 + 2)
#define UWSGI_KSV_LEN(k, v) (sizeof(k) -1 + (v)->len + 2 + 2)

/*
 * Return:
 *  OK: done send request to uwsgi
 *  others: errors
 */
int uwsgi_start_request(connection_t* c) {
    return ERROR;
    /*
    request_t* request = &c->request;
    response_t* response = request->response;
    assert(request->pass);
    uri_t* uri = &request->uri;
    request_headers_t* headers = &request->headers;
    string_t script_name = string_null;
    string_t path_info = string_null;
    script_name = STRING("");
    path_info = uri->abs_path;

    int err;
    buffer_t buf;
    buffer_init(&buf);
    buffer_append_u32le(&buf, 0);
    // Environments
    if (request->method == M_GET) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("GET"));
    } else if (request->method == M_POST) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("POST"));
    } else {
        // TODO(wgtdkp): support more method
        err = response_build_err(response, request, 405);
        goto error;
    }
    uwsgi_buffer_append_kv(&buf, &STRING("SCRIPT_NAME"), &script_name);
    uwsgi_buffer_append_kv(&buf, &STRING("PATH_INFO"), &path_info);
    uwsgi_buffer_append_kv(&buf, &STRING("QUERY_STRING"), &uri->query);
    uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_TYPE"), &headers->content_type);
    uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_LENGTH"), &headers->content_length);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_NAME"), &uri->host);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PORT"), &uri->port);
    uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PROTOCOL"), &STRING("HTTP/1.1"));
    
    // TODO(wgtdkp): more headers
    uwsgi_buffer_append_kv(&buf, &STRING("HTTP_HOST"), &headers->host);
    if (headers->cookie.len)
        uwsgi_buffer_append_kv(&buf, &STRING("HTTP_COOKIE"), &headers->cookie);
    
    char* p = buf.begin + 1;
    uint16_t data_size = buffer_size(&buf) - 4;
    *p++ = data_size & 0xff;
    *p++ = (data_size >> 8) & 0xff;

    // Blocking send
    // As the request from the client is buffered by julia,
    // blocking send should be fine.
    // So the return value could only be OK or ERROR
    //print_buffer(&buf);
    if (buffer_send(&buf, response->resource_fd) != OK) {
        err = response_build_err(response, request, 404);
        goto error;
    }

    response->body_len = request->content_length;
    // Try send body
    if (buffer_size(&request->buffer) >= response->body_len) {
        // The whole body received
        request->body_received = response->body_len;
        request->stage = RS_REQUEST_LINE;
        if (buffer_send(&request->buffer, response->resource_fd) != OK) {
            err = response_build_err(response, request, 404);
            goto error;
        }
    } else {
        assert(response->body_fd == -1);
        // Received body is too long, we need to put it into a tmpfile
        if ((response->body_fd = open("./", O_RDWR | O_TMPFILE, 0x777)) == -1) {
            // TODO(wgtdkp): handle the error
            perror("open");
            assert(false);
        }
        // Significant overhead！
        // It's better to choose BUF_SIZE as the page size of the system 
        request->body_received += buffer_size(&request->buffer);
        write(response->body_fd, request->buffer.begin, buffer_size(&request->buffer));
        request->buffer.end = request->buffer.begin;
    }
        


    set_nonblocking(response->resource_fd);
    back_connection_t* back_connection = pool_alloc(&back_connection_pool);
    back_connection->fd = response->resource_fd;
    back_connection->side = C_SIDE_BACK;
    back_connection->response = response;
    julia_epoll_event_t event = {
        .events = EVENTS_IN,
        .data.ptr = back_connection
    };
    // Send body file the event driven way
    if (response->body_fd != -1) {
        event.events |= EVENTS_OUT;
    }

    // Register event in
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, response->resource_fd, &event) == -1) {
        err = response_build_err(response, request, 404);
        goto error;
    }
    return OK;
error:
    if (response->body_fd != -1) {
        close(response->body_fd);
        response->body_fd = -1;
    }
    close(response->resource_fd);
    response->resource_fd = -1;
    response->fetch_from_back = false;
    return err;
    */
}

int uwsgi_abort_request() {
    return OK;
}

int uwsgi_open_connection(request_t* r, location_t* loc) {
    assert(loc->pass);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ERR_ON(fd == -1, "socket");

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(loc->port);
    int success = inet_pton(AF_INET, loc->host.data, &addr.sin_addr);
    if (success <= 0) return -1;
    
    int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err < 0) return -1;

    r->uc = pool_alloc(&connection_pool);
    connection_register(r->uc);
    r->uc->active_time = time(NULL);

    r->uc->fd = fd;
    r->uc->side = C_SIDE_BACK;

    set_nonblocking(r->uc->fd);
    r->uc->event.events = EVENTS_IN;
    r->uc->event.data.ptr = r->uc;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, r->uc->fd, &r->uc->event) == -1) {
        // It will be closed later
        //close_connection(r->uc);
        return -1;
    }
    r->uc->r = r;
    return fd;
}
