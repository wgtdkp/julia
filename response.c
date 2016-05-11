#include "response.h"
#include "buffer.h"
#include "request.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <strings.h>

#define SERVER_NAME     "julia/0.1"


// The key of (extension, mime-type) pair should be kept in lexicographical order.
// As we will use simple binary search on this array to get the specifical mime-type.
// A little more attention when add more (extension, mime-type) pairs.
static const char* mime_map[][2] = {
    {"css",     "text/css"},
    {"htm",     "text/html"},
    {"html",    "text/html"},
    {"gif",     "image/gif"},
    {"ico",     "image/x-icon"},
    {"jpeg",    "image/jpeg"},
    {"jpg",     "image/jpeg"},
    {"svg",     "image/svg+xml"},
    {"txt",     "text/plain"},
    {"zip",     "application/zip"},
};
static const size_t mime_num = sizeof(mime_map) / sizeof(mime_map[0]);

static char err_page_tail[] =
    "<hr><center>" SERVER_NAME "</center>" CRLF
    "</body>" CRLF
    "</html>" CRLF;

static char err_301_page[] =
    "<html>" CRLF
    "<head><title>301 Moved Permanently</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>301 Moved Permanently</h1></center>" CRLF;

static char err_302_page[] =
    "<html>" CRLF
    "<head><title>302 Found</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>302 Found</h1></center>" CRLF;

static char err_303_page[] =
    "<html>" CRLF
    "<head><title>303 See Other</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>303 See Other</h1></center>" CRLF;

static char err_307_page[] =
    "<html>" CRLF
    "<head><title>307 Temporary Redirect</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>307 Temporary Redirect</h1></center>" CRLF;

static char err_400_page[] =
    "<html>" CRLF
    "<head><title>400 Bad Request</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF;

static char err_401_page[] =
    "<html>" CRLF
    "<head><title>401 Authorization Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>401 Authorization Required</h1></center>" CRLF;

static char err_402_page[] =
    "<html>" CRLF
    "<head><title>402 Payment Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>402 Payment Required</h1></center>" CRLF;

static char err_403_page[] =
    "<html>" CRLF
    "<head><title>403 Forbidden</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>403 Forbidden</h1></center>" CRLF;

static char err_404_page[] =
    "<html>" CRLF
    "<head><title>404 Not Found</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>404 Not Found</h1></center>" CRLF;

static char err_405_page[] =
    "<html>" CRLF
    "<head><title>405 Not Allowed</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>405 Not Allowed</h1></center>" CRLF;

static char err_406_page[] =
    "<html>" CRLF
    "<head><title>406 Not Acceptable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>406 Not Acceptable</h1></center>" CRLF;

static char err_407_page[] =
    "<html>" CRLF
    "<head><title>407 Proxy Authentication Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>407 Proxy Authentication Required</h1></center>" CRLF;

static char err_408_page[] =
    "<html>" CRLF
    "<head><title>408 Request Time-out</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>408 Request Time-out</h1></center>" CRLF;

static char err_409_page[] =
    "<html>" CRLF
    "<head><title>409 Conflict</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>409 Conflict</h1></center>" CRLF;

static char err_410_page[] =
    "<html>" CRLF
    "<head><title>410 Gone</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>410 Gone</h1></center>" CRLF;

static char err_411_page[] =
    "<html>" CRLF
    "<head><title>411 Length Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>411 Length Required</h1></center>" CRLF;

static char err_412_page[] =
    "<html>" CRLF
    "<head><title>412 Precondition Failed</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>412 Precondition Failed</h1></center>" CRLF;

static char err_413_page[] =
    "<html>" CRLF
    "<head><title>413 Request Entity Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>413 Request Entity Too Large</h1></center>" CRLF;

static char err_414_page[] =
    "<html>" CRLF
    "<head><title>414 Request-URI Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>414 Request-URI Too Large</h1></center>" CRLF;

static char err_415_page[] =
    "<html>" CRLF
    "<head><title>415 Unsupported Media Type</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>415 Unsupported Media Type</h1></center>" CRLF;

static char err_416_page[] =
    "<html>" CRLF
    "<head><title>416 Requested Range Not Satisfiable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>416 Requested Range Not Satisfiable</h1></center>" CRLF;

static char err_417_page[] =
    "<html>" CRLF
    "<head><title>417 Expectation Failed</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>417 Expectation Failed</h1></center>" CRLF;

/*    
static char err_494_page[] =
    "<html>" CRLF
    "<head><title>400 Request Header Or Cookie Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>Request Header Or Cookie Too Large</center>" CRLF;

static char err_495_page[] =
    "<html>" CRLF
    "<head><title>400 The SSL certificate error</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>The SSL certificate error</center>" CRLF;

static char err_496_page[] =
    "<html>" CRLF
    "<head><title>400 No required SSL certificate was sent</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>No required SSL certificate was sent</center>" CRLF;

static char err_497_page[] =
    "<html>" CRLF
    "<head><title>400 The plain HTTP request was sent to HTTPS port</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>The plain HTTP request was sent to HTTPS port</center>" CRLF;
*/

static char err_500_page[] =
    "<html>" CRLF
    "<head><title>500 Internal Server Error</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>500 Internal Server Error</h1></center>" CRLF;

static char err_501_page[] =
    "<html>" CRLF
    "<head><title>501 Not Implemented</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>501 Not Implemented</h1></center>" CRLF;

static char err_502_page[] =
    "<html>" CRLF
    "<head><title>502 Bad Gateway</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>502 Bad Gateway</h1></center>" CRLF;

static char err_503_page[] =
    "<html>" CRLF
    "<head><title>503 Service Temporarily Unavailable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>503 Service Temporarily Unavailable</h1></center>" CRLF;

static char err_504_page[] =
    "<html>" CRLF
    "<head><title>504 Gateway Time-out</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>504 Gateway Time-out</h1></center>" CRLF;

static char err_507_page[] =
    "<html>" CRLF
    "<head><title>507 Insufficient Storage</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>507 Insufficient Storage</h1></center>" CRLF;


static char* err_page(int status, int* len);
static const char* status_repr(int status);

/*
static const char* get_mime_type(const char* ext);;
static const char* status_repr(int status);

// Binary search to get corresponding mime-type,
// return NULL when the extension is not supported.
static const char* get_mime_type(const char* ext)
{
    int begin = 0, end = mime_num;
    while (begin <= end) {
        int mid = (end - begin) / 2 + begin;
        int res = strcasecmp(ext, mime_map[mid][0]);
        if (res == 0)
            return mime_map[mid][1];
        else if (res > 0)
            begin = mid + 1;
        else
            end = mid - 1;
    }
    return NULL;
}

*/

void response_init(response_t* response)
{
    response->status = 200;
    response->must_close = false;
    buffer_init(&response->buffer);
}

// Return: 1: response all sent; 0: partial sent;
int put_response(connection_t* connection)
{
    request_t* request = &connection->request;
    response_t* response = &connection->response;
    buffer_t* buffer = &response->buffer;
    
    assert(buffer_size(buffer) == 0);
    
    buffer_send(buffer, connection->fd);
    if (buffer_size(buffer) == 0) { // All data has been sent
        connection_block_response(connection);
        if (!request->keep_alive)
            connection_close(connection);
        return 1;
    }
    return 0;
}

void response_build_err(response_t* response, request_t* request, int err)
{
    int appended = 0;
    buffer_t* buffer = &response->buffer;
    response->status = err;

    // To make things simple
    // We ensure that the buffer can contain those headers and body
    buffer_print(buffer, "HTTP/1.1 %3d %s" CRLF, response->status,
            status_repr(response->status));
    buffer_append_cstring(buffer, "Server: " SERVER_NAME CRLF);
    if (request->keep_alive) {
        buffer_append_cstring(buffer, "Connection: keep-alive" CRLF);
    } else {
        buffer_append_cstring(buffer, "Connection: close" CRLF);
    }
    buffer_append_cstring(buffer, "Content-Type: text/html" CRLF);
    
    int page_len;
    int page_tail_len = sizeof(err_page_tail) - 1;
    char* page = err_page(response->status, &page_len);
    if (page != NULL) {
        buffer_print(buffer, "Content-Length: %d" CRLF,
                page_len + page_tail_len);
    }
    
    appended = buffer_append_cstring(buffer, CRLF);
    assert(appended == strlen(CRLF));
    
    if (page != NULL) {
        buffer_append_string(buffer, string_settol(page, page_len));
        appended = buffer_append_string(buffer,
                string_settol(err_page_tail, page_tail_len));
        assert(appended == page_tail_len);
    }
}

static char* err_page(int status, int* len)
{
    switch(status) {
    case 100:
    case 101:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 300:
        return NULL;
    
#   define ERR_CASE(err)                        \
    case err:                                   \
        *len = sizeof(err_##err##_page) - 1;    \
        return err_##err##_page;    
    
    ERR_CASE(301)
    ERR_CASE(302)
    ERR_CASE(303)
    case 304: 
    case 305:
        assert(0);
        return NULL;
    ERR_CASE(307)
    ERR_CASE(400)
    ERR_CASE(401)
    ERR_CASE(402)
    ERR_CASE(403)
    ERR_CASE(404)
    ERR_CASE(405)
    ERR_CASE(406)
    ERR_CASE(407)
    ERR_CASE(408)
    ERR_CASE(409)
    ERR_CASE(410)
    ERR_CASE(411)
    ERR_CASE(412)
    ERR_CASE(413)
    ERR_CASE(414)
    ERR_CASE(415)
    ERR_CASE(416)
    ERR_CASE(417)
    ERR_CASE(500)
    ERR_CASE(501)
    ERR_CASE(502)
    ERR_CASE(503)
    ERR_CASE(504)
    ERR_CASE(507)

#   undef ERR_CASE 
    default:
        assert(0);
        *len = 0; 
        return NULL;
    }
    
    return NULL;    // Make compiler happy
}

static const char* status_repr(int status)
{
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Time-out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Large";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested range not satisfiable";
    case 417: return "Expectation Failed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Time-out";
    case 505: return "HTTP Version not supported";
    default:
        assert(0);  
        return NULL;
    }
    
    return NULL;    // Make compile happy
}
