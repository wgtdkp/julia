#include "server.h"


#define SERVER_NAME     "julia/0.1"

static char err_page_tail[] =
    "<hr><center><span style='font-style: italic;'>"
     SERVER_NAME "</span></center>" CRLF
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

static char err_505_page[] =
    "<html>" CRLF
    "<head><title>505 HTTP Version Not Supported</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>505 HTTP Version Not Supported</h1></center>" CRLF;

static char err_507_page[] =
    "<html>" CRLF
    "<head><title>507 Insufficient Storage</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>507 Insufficient Storage</h1></center>" CRLF;


#define MIME_MAP_SIZE       (131)
static map_slot_t mime_map_data[2 * MIME_MAP_SIZE];

static map_t mime_map = {
    .size = MIME_MAP_SIZE,
    .max_size = 2 * MIME_MAP_SIZE,
    .data = mime_map_data,
    .cur = mime_map_data + MIME_MAP_SIZE
};

static string_t mime_tb [][2] = {
    {STRING("htm"),     STRING("text/html")},
    {STRING("html"),    STRING("text/html")},
    {STRING("gif"),     STRING("image/gif")},
    {STRING("ico"),     STRING("image/x-icon")},
    {STRING("jpeg"),    STRING("image/jpeg")},
    {STRING("jpg"),     STRING("image/jpeg")},
    {STRING("png"),     STRING("image/png")},
    {STRING("svg"),     STRING("image/svg+xml")},
    {STRING("txt"),     STRING("text/plain")},
    {STRING("zip"),     STRING("application/zip")},
    {STRING("css"),     STRING("text/css")},
};

static char* err_page(int status, int* len);
static const string_t status_repr(int status);
static void response_put_status_line(request_t* request);
static void response_put_date(request_t* r);

void mime_map_init(void) {
    int n = sizeof(mime_tb) / sizeof(mime_tb[0]);
    for (int i = 0; i < n; ++i) {
        map_val_t val;
        val.mime = mime_tb[i][1];
        map_put(&mime_map, &mime_tb[i][0], &val);
    }
}

/*
void response_init(response_t* response)
{
    response->resource_fd = -1;
    response->body_fd = -1;
    response->body_len = 0;
    response->fetch_from_back = false;
    response->status = 200;
    memset(&response->headers, 0, sizeof(response->headers));
    
    response->keep_alive = 1;
    buffer_init(&response->buffer);
}
*/
/*
void response_clear(response_t* response) {
    if (response->resource_fd != -1)
        close(response->resource_fd);
}
*/

int response_build(request_t* r) {
    buffer_t* b = &r->sb;
    
    response_put_status_line(r);
    response_put_date(r);
    buffer_append_cstring(b, "Server: " SERVER_NAME CRLF);
    
    // TODO(wgtdkp): Cache-control or Exprires
    switch (r->status) {
    case 304:
        // 304 has no body
        if (r->resource_fd != -1) {
            close(r->resource_fd);
            r->resource_fd = -1;
        }
    
        buffer_append_cstring(b, CRLF);
        return OK;
    case 100:
        // 100 has no body
        if (r->resource_fd != -1) {
            close(r->resource_fd);
            r->resource_fd = -1;
        }
        
        buffer_append_cstring(b, CRLF);
        return OK;
    default:
        break;
    }
    
    string_t content_type = STRING("text/html");
    map_slot_t* slot = map_get(&mime_map, &r->uri.extension);
    if (slot) {
        content_type = slot->val.mime;
    }
    buffer_append_cstring(b, "Content-Type: ");
    buffer_append_string(b, &content_type);
    buffer_append_cstring(b, CRLF);
    
    buffer_print(b, "Content-Length: %d" CRLF,
                 r->resource_len);
    
    buffer_append_cstring(b, CRLF);
    return OK;
}

static void response_put_status_line(request_t* r) {
    buffer_t* b = &r->sb;
    string_t version;
    if (r->version.minor == 1) {
        version = STRING("HTTP/1.1 ");
    } else {
        version = STRING("HTTP/1.0 ");
    }
    
    buffer_append_string(b, &version);
    string_t status = status_repr(r->status);
    buffer_append_string(b, &status);
    buffer_append_cstring(b, CRLF);
}

static void response_put_date(request_t* r) {
    buffer_t* b = &r->sb;
    
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    b->end += strftime(b->end, buffer_margin(b),
                       "Date: %a, %d %b %Y %H:%M:%S GMT" CRLF, tm);
}

int response_build_err(request_t* r, int err) {
    buffer_t* b = &r->sb;
    //buffer_t* buffer = &response->buffer;
    r->status = err;

    // To make things simple
    // We ensure that the buffer can contain those headers and body
    response_put_status_line(r);
    response_put_date(r);
    buffer_append_cstring(b, "Server: " SERVER_NAME CRLF);
   
    r->keep_alive = false;
    buffer_append_cstring(b, "Connection: close" CRLF);
    buffer_append_cstring(b, "Content-Type: text/html" CRLF);
    
    int page_len;
    int page_tail_len = sizeof(err_page_tail) - 1;
    char* page = err_page(r->status, &page_len);
    if (page != NULL) {
        buffer_print(b, "Content-Length: %d" CRLF,
                page_len + page_tail_len);
    }
    
    buffer_append_cstring(b, CRLF);
    if (page != NULL) {
        buffer_append_string(b, &(string_t){page, page_len});
        buffer_append_string(b, &(string_t){err_page_tail, page_tail_len});
    }
    connection_disable_in(r->c);
    connection_enable_out(r->c);
    return ERROR;
}

static char* err_page(int status, int* len) {
#   define ERR_CASE(err)                        \
    case err:                                   \
        *len = sizeof(err_##err##_page) - 1;    \
        return err_##err##_page;    

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
   
    ERR_CASE(301)
    ERR_CASE(302)
    ERR_CASE(303)
    case 304: 
    case 305:
        assert(false);
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
    ERR_CASE(505)
    ERR_CASE(507)

#   undef ERR_CASE 
    
    default:
        assert(false);
        *len = 0; 
        return NULL;
    }
    
    return NULL;    // Make compiler happy
}

static const string_t status_repr(int status) {
    switch (status) {
    case 100: return STRING("100 Continue");
    case 101: return STRING("101 Switching Protocols");
    case 200: return STRING("200 OK");
    case 201: return STRING("201 Created");
    case 202: return STRING("202 Accepted");
    case 203: return STRING("203 Non-Authoritative Information");
    case 204: return STRING("204 No Content");
    case 205: return STRING("205 Reset Content");
    case 206: return STRING("206 Partial Content");
    case 300: return STRING("300 Multiple Choices");
    case 301: return STRING("301 Moved Permanently");
    case 302: return STRING("302 Found");
    case 303: return STRING("303 See Other");
    case 304: return STRING("304 Not Modified");
    case 305: return STRING("305 Use Proxy");
    case 307: return STRING("307 Temporary Redirect");
    case 400: return STRING("400 Bad Request");
    case 401: return STRING("401 Unauthorized");
    case 402: return STRING("402 Payment Required");
    case 403: return STRING("403 Forbidden");
    case 404: return STRING("404 Not Found");
    case 405: return STRING("405 Method Not Allowed");
    case 406: return STRING("406 Not Acceptable");
    case 407: return STRING("407 Proxy Authentication Required");
    case 408: return STRING("408 Request Time-out");
    case 409: return STRING("409 Conflict");
    case 410: return STRING("410 Gone");
    case 411: return STRING("411 Length Required");
    case 412: return STRING("412 Precondition Failed");
    case 413: return STRING("413 Request Entity Too Large");
    case 414: return STRING("414 Request-URI Too Large");
    case 415: return STRING("415 Unsupported Media Type");
    case 416: return STRING("416 Requested range not satisfiable");
    case 417: return STRING("417 Expectation Failed");
    case 500: return STRING("500 Internal Server Error");
    case 501: return STRING("501 Not Implemented");
    case 502: return STRING("502 Bad Gateway");
    case 503: return STRING("503 Service Unavailable");
    case 504: return STRING("504 Gateway Time-out");
    case 505: return STRING("505 HTTP Version not supported");
    default:
        assert(false);  
        return string_null;
    }
    
    return string_null;    // Make compile happy
}
