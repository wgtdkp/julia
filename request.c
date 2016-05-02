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


/*
 * Request
 */

void request_init(request_t* request)
{
    request->status = 400;
    request->method = M_GET;    // Any value is ok
    request->version.major = 0;
    request->version.minor = 0;

    memset(&request->headers, 0, sizeof(request->headers));
    
    string_init(&request->request_line);
    string_init(&request->uri);
    string_init(&request->extension);
    string_init(&request->schema);
    string_init(&request->host);
    string_init(&request->header_name);
    string_init(&request->header_value);
    request->header_hash = 0;

    request->invalid_header = false;

    request->stage = 0;
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
    request_t* request = &connection->request;
    buffer_t* buffer = &request->buffer;
    bool client_closed = false;

    int readed = buffer_read(connection->fd, buffer);
    
    // Client closed the connection
    if (readed <= 0) {
        readed = -readed;
        client_closed = true;
        // TODO(wgtdkp): remove EPOLLINT events
    }

    if (buffer_size(buffer) > 0)
        print_string("%*s", (string_t){buffer->begin, buffer->end});

    int err = request_parse(request);
    if (err != OK && err != AGAIN) {
        // TODO(wgtdkp): update request state
        // Response bad request
        printf("err: %d\n", err);
        fflush(stdout);
        connection_close(connection);
    }

    // TODO(wgtdkp): process headers

    if (client_closed) {
        printf("connection closed\n");
        fflush(stdout);
        connection_close(connection);
    }

    // TODO(wgtdkp): request done, 
    // send response directly, until send buffer is full.
    
    return 0;
}
