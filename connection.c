#include "connection.h"


void request_init(void)
{
    Request request;
    request.method = M_GET;
    request.version[0] = 1;
    request.version[1] = 1;
    request.query_string = string_init(0);
    request.path = string_init(0);
    request.keep_alive = true;
    return request;
}

void request_release(Request* request)
{
    string_release(&request->query_string);
    string_release(&request->path);
}
