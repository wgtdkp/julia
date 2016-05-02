/* 
 * Based on nginx ngx_http_parse.c
 */

#include "parse.h"
#include "buffer.h"
#include "map.h"
#include "request.h"
#include "string.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define STR2_EQ(p, q)   ((p)[0] == (q)[0] && (p)[1] == (q)[1])
#define STR3_EQ(p, q)   (STR2_EQ(p, q) && (p)[2] == (q)[2])
#define STR4_EQ(p, q)   (STR2_EQ(p, q) && STR2_EQ(p + 2, q + 2))
#define STR5_EQ(p, q)   (STR2_EQ(p, q) && STR3_EQ(p + 2, q + 2))
#define STR6_EQ(p, q)   (STR3_EQ(p, q) && STR3_EQ(p + 3, q + 3))
#define STR7_EQ(p, q)   (STR3_EQ(p, q) && STR4_EQ(p + 3, q + 3))

#define ERR_ON(cond, err)   \
do {                        \
    if (cond)               \
        return (err);       \
} while (0)

/*
static const char token_tb[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, // 7
    0, 0, 0, 0, 0, 0, 0, 0, // 15
    0, 0, 0, 0, 0, 0, 0, 0, // 23
    0, 0, 0, 0, 0, 0, 0, 0, // 31
    0, 0, 0, 0, 0, 0, 0, 0, // 39
    0, 0, 0, 0, 0, 0, 0, 0, // 47
    0, 0, 0, 0, 0, 0, 0, 0, // 55
    0, 0, 0, 0, 0, 0, 0, 0, // 63
    0, 1, 1, 1, 1, 1, 1, 1, // 71
    1, 1, 0, 0, 0, 0, 0, 0, // 79
    0, 0, 0, 0, 0, 0, 0, 0, // 87
    0, 0, 0, 0, 0, 0, 0, 0, // 95
    0, 0, 0, 0, 0, 0, 0, 0, // 103
    0, 0, 0, 0, 0, 0, 0, 0, // 111
    0, 0, 0, 0, 0, 0, 0, 0, // 119
    0, 0, 0, 0, 0, 0, 0, 0, // 127
};
*/

// State machine: request line states
enum {
    // Request line states
    RL_S_BEGIN = 0,
    RL_S_METHOD,
    RL_S_SP_BEFORE_URI,
    RL_S_URI,
    RL_S_SP_BEFROE_VERSION,
    RL_S_HTTP_H,
    RL_S_HTTP_HT,
    RL_S_HTTP_HTT,
    RL_S_HTTP_HTTP,
    RL_S_HTTP_VERSION_SLASH,
    RL_S_HTTP_VERSION_MAJOR,
    RL_S_HTTP_VERSION_DOT,
    RL_S_HTTP_VERSION_MINOR,
    RL_S_SP_AFTER_VERSION,
    RL_S_ALMOST_DONE,
    RL_S_DONE,

    // Header line states
    HL_S_BEGIN,
    HL_S_IGNORE,
    HL_S_NAME,
    HL_S_COLON,
    HL_S_SP_BEFORE_VALUE,
    HL_S_VALUE,
    HL_S_SP_AFTER_VALUE,
    HL_S_ALMOST_DONE,
    HL_S_DONE,

    // URI states
    URI_S_BEGIN,
    URI_S_SCHEME,
    URI_S_SCHEME_COLON,
    URI_S_SCHEME_SLASH,
    URI_S_SCHEME_SLASH_SLASH,
    URI_S_HOST,
    URI_S_PORT,
    URI_S_ABS_PATH,
    URI_S_EXTENSION,
    URI_S_QUERY,
};


static int parse_uri(request_t* request, char* p);
static int parse_method(char* begin, char* end);
static int parse_request_line(request_t* request);
static int parse_header_line(request_t* request);
static int parse_request_body(request_t* request);
static void put_header(request_t* request);

int request_parse(request_t* request)
{
    int res = 0;
    if (request->stage == RS_REQUEST_LINE) {
        res = parse_request_line(request);
        if (res == OK) {
            request->stage = RS_HEADERS;
            // TODO(wgtdkp): process request line
        } else {    // AGAIN or ERR
            return res;
        }
    }

    if (request->stage == RS_HEADERS) {
        while (true) {
            res = parse_header_line(request);
            switch (res) {
            case AGAIN:
                return AGAIN;
            case EMPTY_LINE:
                request->stage = RS_BODY;
                goto parse_body;
            case OK:
                put_header(request);
                break; 
            default:
                assert(0);
            }
        }
    }

parse_body:
    if (request->stage == RS_BODY) {
        res = parse_request_body(request);  
    }

    return res;
}

static void put_header(request_t* request)
{
    int offset = header_offset(request->header_hash, request->header_name);
    if (offset < 0) {   // Can't recognize this header
        // TODO(wgtdkp): log it!
        
    } else {
        string_t* member = (string_t*)((void*)&request->headers + offset);
        *member = request->header_value;
    }
}

static inline int parse_method(char* begin, char* end)
{
    // End of method
    int len = end - begin;
    switch (len) {
    case 3:
        if (STR3_EQ(begin, "GET"))
            return M_GET;
        else if(STR3_EQ(begin, "PUT"))
            return M_PUT;
        else
            return ERR_INVALID_METHOD;
        break;
    case 4:
        if (STR4_EQ(begin, "POST"))
            return M_POST;
        else if (STR4_EQ(begin, "HEAD"))
            return M_HEAD;
        else
            return ERR_INVALID_METHOD;
        break;
    case 5:
        if (STR5_EQ(begin, "TRACE"))
            return M_TRACE;
        else
            return ERR_INVALID_METHOD;
        break;
    case 6:
        if (STR6_EQ(begin, "DELETE"))
            return M_DELETE;
        else
            return ERR_INVALID_METHOD;
        break;
    case 7:
        if (STR7_EQ(begin, "CONNECT"))
            return M_CONNECT;
        else if (STR7_EQ(begin, "OPTIONS"))
            return M_OPTIONS;
        else
            return ERR_INVALID_METHOD;
        break;
    default:
        return ERR_INVALID_METHOD;
    }
    return ERR_INVALID_METHOD;   
}

static int parse_request_line(request_t* request)
{
    buffer_t* buffer = &request->buffer;
    int uri_err;
    char* p;
    for (p = buffer->begin; p < buffer->end; p++) {
        char ch = *p;
        
        switch (request->state) {
        case RL_S_BEGIN:
            switch (ch) {
            case 'A' ... 'Z':
                request->request_line.begin = p;
                request->state = RL_S_METHOD;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_METHOD:
            switch (ch) {
            case 'A' ... 'Z':
                break;
            case ' ':
                request->method = parse_method(request->request_line.begin, p);
                if (request->method == ERR_INVALID_METHOD)
                    return request->method;
                request->state = RL_S_SP_BEFORE_URI;
                break;
            }
            break;

        case RL_S_SP_BEFORE_URI:
            switch (ch) {
            case '\t':
            case '\r':
            case '\n':
                return ERR_INVALID_REQUEST;
            case ' ':
                break;
            default:
                request->uri.state = URI_S_BEGIN;
                if ((uri_err = parse_uri(request, p)) != OK)
                    return uri_err;
                request->state = RL_S_URI;
                break;
            }
            break;

        case RL_S_URI:
            switch (ch) {
            case '\t':
            case '\r':
            case '\n':
                return ERR_INVALID_REQUEST;
            case ' ':
                request->state = RL_S_SP_BEFROE_VERSION;
                // Fall through
            default: 
                if ((uri_err = parse_uri(request, p)) != OK)
                    return uri_err;
                break;
            }
            break;

        case RL_S_SP_BEFROE_VERSION:
            switch (ch) {
            case ' ':
                break;
            case 'H':
                request->state = RL_S_HTTP_H;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        // Is "HTTP/major.minor" case-sensitive?
        case RL_S_HTTP_H:
            ERR_ON(ch != 'T', ERR_INVALID_REQUEST);
            request->state = RL_S_HTTP_HT;
            break;

        case RL_S_HTTP_HT:
            ERR_ON(ch != 'T', ERR_INVALID_REQUEST);
            request->state = RL_S_HTTP_HTT;
            break;

        case RL_S_HTTP_HTT:
            ERR_ON(ch != 'P', ERR_INVALID_REQUEST);
            request->state = RL_S_HTTP_HTTP;
            break;

        case RL_S_HTTP_HTTP:
            ERR_ON(ch != '/', ERR_INVALID_REQUEST);
            request->state = RL_S_HTTP_VERSION_SLASH;
            break;

        case RL_S_HTTP_VERSION_SLASH:
            switch (ch) {
            case '0' ... '9':
                request->version.major =
                        request->version.major * 10 + ch - '0';
                request->state = RL_S_HTTP_VERSION_MAJOR;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_MAJOR:
            switch (ch) {
            case '0' ... '9':
                request->version.major =
                        request->version.major * 10 + ch - '0';
                ERR_ON(request->version.major > 999, ERR_INVALID_VERSION);
                break;
            case '.':
                request->state = RL_S_HTTP_VERSION_DOT;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_DOT:
            switch (ch) {
            case '0' ... '9':
                request->version.minor =
                        request->version.minor * 10 + ch - '0';
                request->state = RL_S_HTTP_VERSION_MINOR;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_MINOR:
            switch (ch) {
            case '0' ... '9':
                request->version.minor =
                        request->version.minor * 10 + ch - '0';
                ERR_ON(request->version.minor > 999, ERR_INVALID_VERSION);
                break;
            case ' ':
                request->state = RL_S_SP_AFTER_VERSION;
                break;
            case '\r':
                request->state = RL_S_ALMOST_DONE;
                break;
            case '\n':
                goto done;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_SP_AFTER_VERSION:
            switch (ch) {
            case ' ':
                break;
            case '\r':
                request->state = RL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_ALMOST_DONE:
            ERR_ON(ch != '\n', ERR_INVALID_REQUEST);
            goto done;

        default:
            assert(0);
        }
    }

    buffer->begin = buffer->end;
    return AGAIN;

done:
    buffer->begin = p + 1;
    request->request_line.end = p;
    request->state = HL_S_BEGIN;
    return OK;
}

/*
 * Request-URI = [scheme ":" "//" host[":" port]][abs_path["?" query]]
 */
static int parse_uri(request_t* request, char* p)
{ 
    char ch = *p;
    switch (request->uri.state) {
    case URI_S_BEGIN:
        switch (ch) {
        case '/':
            request->uri.abs_path.begin = p;
            request->uri.state = URI_S_ABS_PATH;
            break;
        case 'A' ... 'Z':
        case 'a' ... 'z':
            request->uri.scheme.begin = p;
            request->uri.state = URI_S_SCHEME;
            break; 
        default:
            return ERR_INVALID_URI;
        }
        break;

    case URI_S_SCHEME:
        switch (ch) {
        case 'A' ... 'Z':
        case 'a' ... 'z':
        case '0' ... '9':
        case '+':
        case '-':
        case '.':
            break;
        case ':':
            request->uri.scheme.end = p;
            request->uri.state = URI_S_SCHEME_COLON;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_COLON:
        switch (ch) {
        case '/':
            request->uri.state = URI_S_SCHEME_SLASH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_SLASH:
        switch (ch) {
        case '/':
            request->uri.state = URI_S_SCHEME_SLASH_SLASH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_SLASH_SLASH:
        switch (ch) {
        case 'A' ... 'Z':
        case 'a' ... 'z':
        case '0' ... '9':
            request->uri.host.begin = p;
            request->uri.state = URI_S_HOST;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;

    case URI_S_HOST:
        switch (ch) {
        case 'A' ... 'Z':
        case 'a' ... 'z':
        case '0' ... '9':
        case '.':
        case '-':
            break;
        case ':':
            request->uri.state = URI_S_PORT;
            break;
        case '/':
            request->uri.abs_path.begin = p;
            request->uri.state = URI_S_ABS_PATH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
  
    case URI_S_PORT:
        switch (ch) {
        case ' ':
            request->uri.abs_path.begin = request->uri.host.begin - 1;
            request->uri.abs_path.end = request->uri.host.begin;

            ++request->uri.port.begin;
            request->uri.port.end = p;
            request->uri.state = URI_S_BEGIN;
            break;
        case '/':
            request->uri.abs_path.begin = p;

            ++request->uri.port.begin;
            request->uri.port.end = p;
            request->uri.state = URI_S_ABS_PATH;
            break;
        case '0' ... '9':
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;

#   define ALLOWED_IN_ABS_PATH      \
             'A' ... 'Z':           \
        case 'a' ... 'z':           \
        case '0' ... '9':           \
        /* Mark */                  \
        case '-':                   \
        case '_':                   \
        case '!':                   \
        case '~':                   \
        case '*':                   \
        case '\'':                  \
        case '(':                   \
        case ')':                   \
        /* Escaped */               \
        case '%':                   \
        case ':':                   \
        case '@':                   \
        case '&':                   \
        case '=':                   \
        case '+':                   \
        case '$':                   \
        case ',':                   \
        case ';'

#   define ALLOWED_IN_EXTENSION     ALLOWED_IN_ABS_PATH

#   define ALLOWED_IN_QUERY         \
             '.':                   \
        case '/':                   \
        case '?':                   \
        case ALLOWED_IN_ABS_PATH

    case URI_S_ABS_PATH:
        switch (ch) {
        case ' ':
            request->uri.abs_path.end = p;
            request->uri.state = URI_S_BEGIN;
            break;
        case '.':
            request->uri.extension.begin = p;
            request->uri.state = URI_S_EXTENSION;
            break;
        case '?':
            request->uri.abs_path.end = p;
            
            request->uri.query.begin = p;
            request->uri.state = URI_S_QUERY;
            break;
        case '/':
        case ALLOWED_IN_ABS_PATH:
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_EXTENSION:
        switch (ch) {
        case ' ':
            ++request->uri.extension.begin;
            request->uri.extension.end = p;
            
            request->uri.state = URI_S_BEGIN;
            break;
        case '.':
            request->uri.extension.begin = p;
            break;
        case '/':
            request->uri.extension.begin = NULL;
            request->uri.state = URI_S_ABS_PATH;
            break;
        case '?':
            ++request->uri.extension.begin;
            request->uri.extension.end = p;
            
            request->uri.query.begin = p;
            request->uri.state = URI_S_QUERY;
            break;
        case ALLOWED_IN_EXTENSION:
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;

    case URI_S_QUERY:
        switch (ch) {
        case ' ':
            ++request->uri.query.begin;
            request->uri.query.end = p;
            
            request->uri.state = URI_S_BEGIN;
            break;
        case ALLOWED_IN_QUERY:
            break;
        default:
            return ERR_INVALID_URI; 
        }
        break;

#   undef ALLOWED_IN_ABS_PATH
#   undef ALLOWED_IN_EXTENSION
#   undef ALLOWED_IN_QUERY

    default:
        assert(0);
    }
    return OK;
}

static int parse_header_line(request_t* request)
{
    buffer_t* buffer = &request->buffer;
    hash_t hash = 0;
    char* p;
    for (p = buffer->begin; p < buffer->end; p++) {
        char ch = *p;
        switch (request->state) {
        case HL_S_BEGIN:
            // Reset states
            string_init(&request->header_name);
            string_init(&request->header_value);
            switch(ch) {
            case 'A' ... 'Z':
                *p = *p - 'A' + 'a';
                ch = *p;
                hash = header_hash(0, ch);
                request->header_name.begin = p;
                request->state = HL_S_NAME;
                break;
            case '-':
                *p = *p - '-' + '_';
                ch = *p;
            case '0' ... '9':
            case 'a' ... 'z':
                hash = header_hash(0, ch);
                request->header_name.begin = p;
                request->state = HL_S_NAME;
                break;
            case '\r':
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                request->state = HL_S_IGNORE;
                break;
            }
            break;

        case HL_S_IGNORE:
            switch (ch) {
            case '\n':
                request->state = HL_S_BEGIN;
                break;
            default:
                break;
            }
            break;

        case HL_S_NAME:
            switch(ch) {
            case 'A' ... 'Z':
                *p = *p - 'A' + 'a';
                ch = *p;
                hash = header_hash(hash, ch);
                break;
            case '-':
                // Convert '-' to '_'
                *p = *p - '-' + '_';
                ch = *p;

                // Fall through
            case '0' ... '9':
            case 'a' ... 'z':
                hash = header_hash(hash, ch);
                break;
            case ':':
                request->header_name.end = p;
                request->state = HL_S_COLON;
                break;
            case '\r':
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                request->state = HL_S_IGNORE;
                break;
            }
            break;

        case HL_S_SP_BEFORE_VALUE: 
        case HL_S_COLON:
            switch (ch) {
            case ' ':
                request->state = HL_S_SP_BEFORE_VALUE;
                break;
            case '\r':
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                goto header_done;
            default:
                request->header_value.begin = p;
                request->state = HL_S_VALUE;
                break;
            }
            break;

        case HL_S_VALUE:
            switch (ch) {
            case ' ':
                request->header_value.end = p;
                request->state = HL_S_SP_AFTER_VALUE;
                break;
            case '\r':
                request->header_value.end = p;
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                request->header_value.end = p;
                goto header_done;
            default:
                break;
            }
            break;

        case HL_S_SP_AFTER_VALUE:
            switch (ch) {
            case ' ':
                break;
            case '\r':
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                // Single '\n' is also accepted
                goto header_done;
            default:
                request->state = HL_S_VALUE;
                break;
            }
            break;

        case HL_S_ALMOST_DONE:
            switch (ch) {
            case '\n':
                goto header_done;
            default:
                request->state = HL_S_IGNORE;
                break;
            }
            break;

        default:
            assert(0);
        }
    }
    
    buffer->begin = buffer->end;
    return AGAIN;

header_done:
    // DEBUG:
    buffer->begin = p + 1;
    request->header_hash = hash;
    request->state = HL_S_BEGIN;
    
    return request->header_name.begin == NULL ? EMPTY_LINE: OK;
}

static int parse_request_body(request_t* request)
{
    return OK;
}

/*

general-header = Cache-Control           ; Section 14.9
              | Connection               ; Section 14.10
              | Date                     ; Section 14.18
              | Pragma                   ; Section 14.32
              | Trailer                  ; Section 14.40
              | Transfer-Encoding        ; Section 14.41
              | Upgrade                  ; Section 14.42
              | Via                      ; Section 14.45
              | Warning                  ; Section 14.46

request-header = Accept                  ; Section 14.1
              | Accept-Charset           ; Section 14.2
              | Accept-Encoding          ; Section 14.3
              | Accept-Language          ; Section 14.4
              | Authorization            ; Section 14.8
              | Expect                   ; Section 14.20
              | From                     ; Section 14.22
              | Host                     ; Section 14.23
              | If-Match                 ; Section 14.24
              | If-Modified-Since        ; Section 14.25
              | If-None-Match            ; Section 14.26
              | If-Range                 ; Section 14.27
              | If-Unmodified-Since      ; Section 14.28
              | Max-Forwards             ; Section 14.31
              | Proxy-Authorization      ; Section 14.34
              | Range                    ; Section 14.35
              | Referer                  ; Section 14.36
              | TE                       ; Section 14.39
              | User-Agent               ; Section 14.43

response-header = Accept-Ranges          ; Section 14.5
               | Age                     ; Section 14.6
               | ETag                    ; Section 14.19
               | Location                ; Section 14.30
               | Proxy-Authenticate      ; Section 14.33
               | Retry-After             ; Section 14.37
               | Server                  ; Section 14.38
               | Vary                    ; Section 14.44
               | WWW-Authenticate        ; Section 14.47

entity-header  = Allow                    ; Section 14.7
                | Content-Encoding         ; Section 14.11
                | Content-Language         ; Section 14.12
                | Content-Length           ; Section 14.13
                | Content-Location         ; Section 14.14
                | Content-MD5              ; Section 14.15
                | Content-Range            ; Section 14.16
                | Content-Type             ; Section 14.17
                | Expires                  ; Section 14.21
                | Last-Modified            ; Section 14.29
                | extension-header
extension-header = message-header
*/


/* URI Generic syntax
    URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
    absoluteURI   = scheme ":" ( hier_part | opaque_part )
    relativeURI   = ( net_path | abs_path | rel_path ) [ "?" query ]

    hier_part     = ( net_path | abs_path ) [ "?" query ]
    opaque_part   = uric_no_slash *uric

    uric_no_slash = unreserved | escaped | ";" | "?" | ":" | "@" |
                  "&" | "=" | "+" | "$" | ","

    net_path      = "//" authority [ abs_path ]
    abs_path      = "/"  path_segments
    rel_path      = rel_segment [ abs_path ]

    rel_segment   = 1*( unreserved | escaped |
                      ";" | "@" | "&" | "=" | "+" | "$" | "," )

    scheme        = alpha *( alpha | digit | "+" | "-" | "." )

    authority     = server | reg_name

    reg_name      = 1*( unreserved | escaped | "$" | "," |
                      ";" | ":" | "@" | "&" | "=" | "+" )

    server        = [ [ userinfo "@" ] hostport ]
    userinfo      = *( unreserved | escaped |
                     ";" | ":" | "&" | "=" | "+" | "$" | "," )

    hostport      = host [ ":" port ]
    host          = hostname | IPv4address
    hostname      = *( domainlabel "." ) toplabel [ "." ]
    domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
    toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
    IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
    port          = *digit

    path          = [ abs_path | opaque_part ]
    path_segments = segment *( "/" segment )
    segment       = *pchar *( ";" param )
    param         = *pchar
    pchar         = unreserved | escaped |
                  ":" | "@" | "&" | "=" | "+" | "$" | ","

    query         = *uric

    fragment      = *uric

    uric          = reserved | unreserved | escaped
    reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
                  "$" | ","
    unreserved    = alphanum | mark
    mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
                  "(" | ")"

    escaped       = "%" hex hex
    hex           = digit | "A" | "B" | "C" | "D" | "E" | "F" |
                          "a" | "b" | "c" | "d" | "e" | "f"

    alphanum      = alpha | digit
    alpha         = lowalpha | upalpha

    lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" |
             "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" |
             "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
    upalpha  = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
             "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
             "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
    digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
             "8" | "9"
*/
