#include "server.h"

static int uwsgi_start_request(int fd, request_t* request);
static int uwsgi_handle_response(int fd, response_t* response);

int uwsgi_open_connection(config_t* cfg)
{
    bool unix_socket = cfg->dynamic_unix_socket;
    string_t* addr = &cfg->dynamic_addr;
    uint16_t port = cfg->dynamic_port;
    int fd;
    if (unix_socket) {
        fd = socket(AF_LOCAL, SOCK_STREAM, 0);
        unlink(addr->data);
        struct sockaddr_un app_addr;
        bzero(&app_addr, sizeof(app_addr));
        app_addr.sun_family = AF_LOCAL;
        // FIXME(wgtdkp): check path length
        strcpy(app_addr.sun_path, addr->data);
        int err = connect(fd, (struct sockaddr*)&app_addr, sizeof(app_addr));
        return err ? -1: fd;
    } else {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in app_addr;
        bzero(&app_addr, sizeof(app_addr));
        app_addr.sin_family = AF_INET;
        app_addr.sin_port = htons(port);
        int err = connect(fd, (struct sockaddr*)&app_addr, sizeof(app_addr));
        return err ? -1: fd;
    }
    return -1;
}


int uwsgi_close_connection(int fd)
{
    close(fd);
    return OK;
}

/*
 * Takeover request from server
 */
int uwsgi_takeover(response_t* response, request_t* request)
{
    //buffer_send(&request->buffer, app_fd);
    //buffer_recv(&response->buffer, app_fd);
    uwsgi_start_request(app_fd, request);
    uwsgi_handle_response(app_fd, response);
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
    script_name.data = uri->abs_path.data;
    if (uri->extension.len != 0) {
        script_name.len = string_end(&uri->extension) - script_name.data;
        path_info.data = string_end(&uri->extension);
        path_info.len = string_end(&uri->abs_path) - string_end(&uri->extension);
    } else {
        //path_info.data =
        assert(false); 
    }

    buffer_t buf;
    buffer_init(&buf);
    uint16_t data_size = 0;
    buffer_append_u32le(&buf, 0);
    // Environments
    if (request->method == M_GET) {
        data_size += uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("GET"));
    } else if (request->method == M_POST) {
        data_size += uwsgi_buffer_append_kv(&buf, &STRING("REQUEST_METHOD"), &STRING("POST"));
    } else {
        assert(false);
    }

    data_size += uwsgi_buffer_append_kv(&buf, &STRING("SCRIPT_NAME"), &script_name);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("PATH_INFO"), &path_info);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("QUERY_STRING"), &uri->query);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_TYPE"), &headers->content_type);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("CONTENT_LENGTH"), &headers->content_length);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("HTTP_HOST"), &headers->host);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("SERVER_NAME"), &uri->host);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PORT"), &uri->port);
    data_size += uwsgi_buffer_append_kv(&buf, &STRING("SERVER_PROTOCOL"), &STRING("HTTP/1.1"));

    char* p = buf.begin + 1;
    *p++ = data_size & 0xff;
    *p++ = (data_size >> 8) & 0xff;
    

    // FIXME(wgtdkp): may blocked
    //while (buffer_size(&buf) > 0)
    buffer_send(&buf, fd);
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

    char* p = buffer->begin;
    printf("%d\n", (uint8_t)*p);
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


