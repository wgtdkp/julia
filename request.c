#include "request.h"
#include "buffer.h"
#include "map.h"
#include "parse.h"
#include "response.h"
#include "server.h"
#include "string.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static int request_process_uri(request_t* request, response_t* response);
static int request_process_request_line(request_t* request, response_t* response);
static int request_process_headers(request_t* request, response_t* response);
static int request_process_body(request_t* request, response_t* response);
static void request_put_header(request_t* request);


/*
 * Request
 */

void request_init(request_t* request)
{
    request->status = 200;
    request->resource_fd = -1;
    request->method = M_GET;    // Any value is ok
    request->version.major = 0;
    request->version.minor = 0;

    memset(&request->headers, 0, sizeof(request->headers));
    
    string_init(&request->request_line);
    string_init(&request->header_name);
    string_init(&request->header_value);
    request->header_hash = 0;
    memset(&request->uri, 0, sizeof(request->uri));

    request->stage = RS_REQUEST_LINE;
    request->state = 0; // RL_S_BEGIN
    request->uri_state = 0; // Meaningless
    request->keep_alive = false;

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
}

int handle_request(connection_t* connection)
{
    int err = 0;
    request_t* request = &connection->request;
    response_t* response = &connection->response;
    buffer_t* buffer = &request->buffer;
    bool client_closed = false;

    int readed = buffer_recv(buffer, connection->fd);
    
    // Client closed the connection
    if (readed <= 0) {
        readed = -readed;
        client_closed = true;
        // TODO(wgtdkp): remove EPOLLINT events
    }
    if (buffer_size(buffer) > 0)
        print_string("%*s", (string_t){buffer->begin, buffer->end});

    if (err == 0 && request->stage == RS_REQUEST_LINE)
        err = request_process_request_line(request, response);
    if (err == 0 && request->stage == RS_HEADERS)
        err = request_process_headers(request, response);
    if (err == 0 && request->stage == RS_BODY)
        err = request_process_body(request, response);

    if (client_closed) {
        printf("connection closed by the client side\n");
        fflush(stdout);
        connection_close(connection);
    }
    
    // TODO(wgtdkp): request done, 
    // send response directly, until send buffer is full.
    
    return OK;
}

static void request_put_header(request_t* request)
{
    int offset = header_offset(request->header_hash, request->header_name);
    if (offset < 0) {   // Can't recognize this header
        // TODO(wgtdkp): log it!
    } else {
        string_t* member = (string_t*)((void*)&request->headers + offset);
        *member = request->header_value;
    }
}

static int request_process_uri(request_t* request, response_t* response)
{
    uri_t* uri = &request->uri;
    *uri->abs_path.end = 0;  // It is safe to do this
    const char* rel_path = uri->abs_path.begin + 1;
    int fd = openat(doc_root_fd, rel_path, O_RDONLY);
    
    // Open the requested resource failed
    if (fd == -1) {
        response_build_err(response, request, 404);
        return ERR_STATUS(response->status);
    }
    
    request->resource_fd = fd;
    return OK;
}

static int request_process_request_line(request_t* request, response_t* response)
{
    int err = parse_request_line(request);
    if (err != OK)  // AGAIN or ERR
        return err;
    request->stage = RS_HEADERS;

    // TODO(wgtdkp): check method
        
    err = request_process_uri(request, response);

    return err;
}

static int request_process_headers(request_t* request, response_t* response)
{
    while (true) {
        int err = parse_header_line(request);
        switch (err) {
        case AGAIN:
            return AGAIN;
        case EMPTY_LINE:
            request->stage = RS_BODY;
            goto done;
        case OK:
            request_put_header(request);
            break; 
        default:
            assert(0);
        }
    }
done:
    // TODO(wgtdkp): process host if needed
    // TODO(wgtdkp): process content-length if needed
    return OK;
}

static int request_process_body(request_t* request, response_t* response)
{
    int err = parse_request_body(request);

    return err;
}
