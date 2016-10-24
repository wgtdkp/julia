#include "server.h"

static int uwsgi_start_request(int fd, request_t* request);
static int uwsgi_handle_response(int fd, response_t* response);

/*
 * Takeover request from server
 */
int uwsgi_takeover(response_t* response, request_t* request)
{
    //buffer_send(&request->buffer, app_fd);
    //buffer_recv(&response->buffer, app_fd);
    backend_open_connection(request->loc);
    int fd = request->loc->fd;
    uwsgi_start_request(fd, request);
    uwsgi_handle_response(fd, response);
    printf("%d\n", buffer_size(&response->buffer)); fflush(stdout);
    //print_buffer(&response->buffer);
    backend_close_connection(request->loc);
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

static int uwsgi_start_request(int fd, request_t* request)
{
    uri_t* uri = &request->uri;
    request_headers_t* headers = &request->headers;
    string_t script_name = string_null;
    string_t path_info = string_null;
    script_name = STRING("");
    path_info = uri->abs_path; // STRING("");
    //script_name.data = uri->abs_path.data;
    //if (uri->extension.len != 0) {
    //    script_name.len = string_end(&uri->extension) - script_name.data;
    //    path_info.data = string_end(&uri->extension);
    //    path_info.len = string_end(&uri->abs_path) - string_end(&uri->extension);
    //} else {
    //    //path_info.data =
    //    assert(false); 
    //}

    buffer_t buf;
    buffer_init(&buf);
    buffer_append_u32le(&buf, 0);
    // Environments
    if (request->method == M_GET) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("GET"));
    } else if (request->method == M_POST) {
        uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("POST"));
    } else {
        assert(false);
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
    

    // FIXME(wgtdkp): may blocked
    //while (buffer_size(&buf) > 0)

    buffer_send(&buf, fd);
    // Send the body
    assert(buffer_size(&request->buffer) == request->body_received);
    buffer_send(&request->buffer, fd);
    return OK;
}

int uwsgi_abort_request()
{
    return OK;
}

static int uwsgi_handle_response(int fd, response_t* response)
{
    buffer_t* buffer = &response->buffer;
    buffer_recv(buffer, fd);

    //char* p = buffer->begin;
    //printf("%d\n", (uint8_t)*p);
    print_buffer(buffer);
    //buffer->begin += 4;
    //buffer_print(buffer);
    return OK;
}

int uwsgi_process_status_line()
{
    return OK;
}

int uwsgi_finalize_request()
{
    return OK;
}

int uwsgi_send_packet()
{
    return OK;
}


