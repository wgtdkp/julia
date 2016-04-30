/* 
 * Based on nginx ngx_http_parse.c
 */

#include "parse.h"
#include "buffer.h"
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

#define IS_DIGIT(ch)    (ch >= '0' && ch <= '9')

#define IS_UPALPHA(ch)  ((ch) >= 'A' && (ch) <= 'Z')
#define IS_LOALPHA(ch)  ((ch) >= 'a' && (ch) <= 'z')
#define IS_ALPHA(ch)    (IS_LOALPHA(ch) || IS_UPALPHA(ch))

#define TO_LOWCASE(ch)  ((ch) | 0x20)


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

static int parse_uri(request_t* request, char ch);
static int parse_request_line(request_t* request);
static int parse_header_line(request_t* request);
static int parse_request_body(request_t* request);


int request_parse(request_t* request)
{
    // 0. parse request line

    int err = 0;
    if (!request->request_line_done) {
        err = parse_request_line(request);
        if (err != OK)
            return err;
    }

    if (request->request_line_done && !request->headers_done) {
        while (buffer_size(&request->buffer) > 0) {
            err = parse_header_line(request);
            if (request->headers_done)
                break;
            if (err != OK)
                return err;
        }
    }

    if (request->headers_done && !request->body_done) {
        err = parse_request_body(request);
        if (err != OK)
            return err;
    }
    
    // 1. parse header lines

    // 2. parse header body
    return err;
}

// State machine: request line states
enum {
    // Request line state
    RL_S_BEGIN = 0,
    RL_S_METHOD,
    RL_S_SP_BEFORE_URI,
    RL_S_URI_SLASH,
    RL_S_URI_ASTERIK,
    RL_S_SCHEMA,
    RL_S_SCHEMA_COLON,
    RL_S_SCHEMA_SLASH,
    RL_S_SCHEMA_SLASH_SLASH,
    RL_S_HOST_IP_LITERAL,
    RL_S_HOST,
    RL_S_HOST_END,
    RL_S_PORT,
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

    // Header line state
    HL_S_BEGIN,
    HL_S_IGNORE,
    HL_S_NAME,
    HL_S_COLON,
    HL_S_SP_BEFORE_VALUE,
    HL_S_VALUE,
    HL_S_SP_AFTER_VALUE,
    HL_S_ALMOST_DONE,
    HL_S_DONE,
};

enum {
    URI_S_SLASH = 0,
    URI_S_ENTRY,

};

static int parse_request_line(request_t* request)
{
    buffer_t* buffer = &request->buffer;
    char* p;
    for (p = buffer->begin; p < buffer->end; p++) {
        char ch = *p;
        
        switch (request->state) {
        case RL_S_BEGIN:
            request->request_line.begin = p;
            request->method_unparsed.begin = p;
            ERR_ON(!IS_UPALPHA(ch), ERR_INVALID_REQUEST);
            request->state = RL_S_METHOD;
            break;

        case RL_S_METHOD:
            if (IS_UPALPHA(ch))
                break;
            ERR_ON(ch != ' ', ERR_INVALID_METHOD);
            
            // End of method
            char* m = request->method_unparsed.begin;
            int len = p - m;
            switch (len) {
            case 3:
                if (STR3_EQ(m, "GET"))
                    request->method = M_GET;
                else if(STR3_EQ(m, "PUT"))
                    request->method = M_PUT;
                else
                    return ERR_INVALID_METHOD;
                break;
            case 4:
                if (STR4_EQ(m, "POST"))
                    request->method = M_POST;
                else if (STR4_EQ(m, "HEAD"))
                    request->method = M_HEAD;
                else
                    return ERR_INVALID_METHOD;
                break;
            case 5:
                if (STR5_EQ(m, "TRACE"))
                    request->method = M_TRACE;
                else
                    return ERR_INVALID_METHOD;
                break;
            case 6:
                if (STR6_EQ(m, "DELETE"))
                    request->method = M_DELETE;
                else
                    return ERR_INVALID_METHOD;
                break;
            case 7:
                if (STR7_EQ(m, "CONNECT"))
                    request->method = M_CONNECT;
                else if (STR7_EQ(m, "OPTIONS"))
                    request->method = M_OPTIONS;
                else
                    return ERR_INVALID_METHOD;
                break;
            default:
                return ERR_INVALID_METHOD;
            }
            request->state = RL_S_SP_BEFORE_URI;
            break;

        case RL_S_SP_BEFORE_URI:
            // RFC 2616:
            // Request-URI    = "*" | absoluteURI | abs_path | authority
            // absoluteURI: FIRST set: {alpha}
            // abs_path:    FIRST set: {'/'}
            // authority ?
            if (ch == '/') {
                request->uri.begin = p;
                request->state = RL_S_URI_SLASH;
                break;
            } else if (ch == '*') {
                request->uri.begin = p;
                request->state = RL_S_URI_ASTERIK;
                break;
            }

            if (IS_ALPHA(ch)) {
                request->schema.begin = p;
                request->state = RL_S_SCHEMA;
                break;
            }
            ERR_ON(ch != ' ', ERR_INVALID_REQUEST);
            break;

        case RL_S_SCHEMA:
            switch (ch) {
            case 'A' ... 'Z':
            case 'a' ... 'z':
            case '0' ... '9':
            case '+':
            case '-':
            case '.':
                break;
            case ':':
                request->schema.end = p;
                request->state = RL_S_SCHEMA_COLON;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_SCHEMA_COLON:
            switch (ch) {
            case '/':
                request->state = RL_S_SCHEMA_SLASH;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_SCHEMA_SLASH:
            switch (ch) {
            case '/':
                request->state = RL_S_SCHEMA_SLASH_SLASH;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_SCHEMA_SLASH_SLASH:
            request->host.begin = p;
            if (ch == '[') {
                request->state = RL_S_HOST_IP_LITERAL;
                break;
            }
            request->state = RL_S_HOST;
            // Fall through
        case RL_S_HOST:
            switch (ch) {
            case 'A' ... 'Z':
            case 'a' ... 'z':
            case '0' ... '9':
            case '.':
            case '-':
                goto end_state_switch;
            }
            // Fall through
        case RL_S_HOST_END:
            request->host.end = p;
            switch (ch) {
            case ':':
                request->state = RL_S_PORT;
                break;
            case '/':
                request->uri.begin = p;
                request->state = RL_S_URI_SLASH;
                break;
            case ' ':
                // Must ended by '/'
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HOST_IP_LITERAL:
            // "[" 1*(digit | ".") "]"
            switch (ch) {
            case '0' ... '9':
            case '.':
                break;
            case ']':
                request->state = RL_S_HOST_END;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_PORT:
            switch (ch) {
            case '0' ... '9':
                break;
            case '/':
                request->uri.begin = p;
                request->state = RL_S_URI_SLASH;
                break;
            case ' ':
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_URI_SLASH:
            switch (ch) {
            case ' ':
                request->uri.end = p;
                request->state = RL_S_SP_BEFROE_VERSION;
                break;
            case '\t':
            case '\r':
            case '\n':
                return ERR_INVALID_REQUEST;
            default:
                ERR_ON(parse_uri(request, ch) != OK,
                        ERR_INVALID_REQUEST);
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

    end_state_switch:;
    }

    buffer->begin = buffer->end;
    return OK;

done:
    buffer->begin = p + 1;
    request->request_line_done = true;
    request->request_line.end = p;
    request->state = HL_S_BEGIN;
    return OK;
}


static int parse_header_line(request_t* request)
{
    buffer_t* buffer = &request->buffer;
    char* p;
    for (p = buffer->begin; p < buffer->end; p++) {
        char ch = *p;
        switch (request->state) {
        case HL_S_BEGIN:
            switch(ch) {
            case '0' ... '9':
            case 'A' ... 'Z':
            case 'a' ... 'z':
            case '-':
                request->header_name.begin = p;
                request->state = HL_S_NAME;
                break;
            case '\r':
                request->headers_done = true;
                request->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                request->invalid_header = true;
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
            case '0' ... '9':
            case 'A' ... 'Z':
            case 'a' ... 'z':
            case '-':
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
                request->invalid_header = true;
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
                request->headers_done = false;
                request->invalid_header = true;
                request->state = HL_S_IGNORE;
                break;
            }
            break;

        default:
            assert(0);
        }
    }
    
    buffer->begin = buffer->end;
    return OK;

header_done:
    // DEBUG:
    if (NULL != request->header_name.begin)
        print_string("name: %*s\n", request->header_name);
    if (NULL != request->header_name.end)
        print_string("value: %*s\n", request->header_value);

    // Reset states
    string_init(&request->header_name);
    string_init(&request->header_value);
    buffer->begin = p + 1;
    request->state = HL_S_BEGIN;
    return OK;
}

static int parse_uri(request_t* request, char ch)
{
    switch (request->uri_state) {
    case URI_S_SLASH:
        switch (ch) {
        case '/':
            break;

        }
    }
    return OK;
}

static int parse_request_body(request_t* request)
{
    
    return OK;
}

/*
general-header = Cache-Control            ; Section 14.9
              | Connection               ; Section 14.10
              | Date                     ; Section 14.18
              | Pragma                   ; Section 14.32
              | Trailer                  ; Section 14.40
              | Transfer-Encoding        ; Section 14.41
              | Upgrade                  ; Section 14.42
              | Via                      ; Section 14.45
              | Warning                  ; Section 14.46

request-header = Accept                   ; Section 14.1
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

response-header = Accept-Ranges           ; Section 14.5
               | Age                     ; Section 14.6
               | ETag                    ; Section 14.19
               | Location                ; Section 14.30
               | Proxy-Authenticate      ; Section 14.33
               | Retry-After             ; Section 14.37
               | Server                  ; Section 14.38
               | Vary                    ; Section 14.44
               | WWW-Authenticate        ; Section 14.47

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
