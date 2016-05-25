/* 
 * Based on nginx ngx_http_parse.c
 */

#include "parse.h"

#include "base/buffer.h"
#include "base/map.h"
#include "base/string.h"

#include "request.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
    URI_S_ABS_PATH_DOT,
    URI_S_ABS_PATH_DDOT,
    URI_S_ABS_PATH_SLASH,
    URI_S_ABS_PATH_ENTRY,
    URI_S_EXTENSION,
    URI_S_QUERY,
};


static int parse_uri(request_t* request, char* p);
static int parse_method(char* begin, char* end);

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

int parse_request_line(request_t* request)
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
                request->uri_state = URI_S_BEGIN;
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
                if (ch == ' ') {
                    if (request->uri.nentries < request->uri.nddots) {
                        request->uri.abs_path.end = request->uri.abs_path.begin + 1;        
                    }
                }
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
    switch (request->uri_state) {
    case URI_S_BEGIN:
        switch (ch) {
        case '/':
            request->uri.abs_path.begin = p;
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        case 'A' ... 'Z':
        case 'a' ... 'z':
            request->uri.scheme.begin = p;
            request->uri_state = URI_S_SCHEME;
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
            request->uri_state = URI_S_SCHEME_COLON;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_COLON:
        switch (ch) {
        case '/':
            request->uri_state = URI_S_SCHEME_SLASH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_SLASH:
        switch (ch) {
        case '/':
            request->uri_state = URI_S_SCHEME_SLASH_SLASH;
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
            request->uri_state = URI_S_HOST;
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
            request->uri_state = URI_S_PORT;
            break;
        case '/':
            request->uri.abs_path.begin = p;
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
  
    case URI_S_PORT:
        switch (ch) {
        case ' ':
            // For example: http://localhost
            // We set uri to the first slash after colon(':')
            request->uri.abs_path.begin = request->uri.host.begin - 2;
            request->uri.abs_path.end = request->uri.host.begin - 1;

            ++request->uri.port.begin;
            request->uri.port.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '/':
            request->uri.abs_path.begin = p;

            ++request->uri.port.begin;
            request->uri.port.end = p;
            request->uri_state = URI_S_ABS_PATH_SLASH;
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
  
    case URI_S_ABS_PATH_SLASH:
        switch (ch) {
        case ' ':
            request->uri.abs_path.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '.':
            request->uri_state = URI_S_ABS_PATH_DOT;
            break;
        case '?':
            request->uri.abs_path.end = p;
            
            request->uri.query.begin = p;
            request->uri_state = URI_S_QUERY;
            break;
        case '/':
            break;
        case ALLOWED_IN_ABS_PATH:
            request->uri_state = URI_S_ABS_PATH_ENTRY;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_ABS_PATH_DOT:
        switch (ch) {
        case ' ':
            request->uri.abs_path.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '.':
            request->uri_state = URI_S_ABS_PATH_DDOT;
            break;
        case '?':
            request->uri.abs_path.end = p;
            
            request->uri.query.begin = p;
            request->uri_state = URI_S_QUERY;
            break;
        case '/':
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            // Extension begins at the '.'
            request->uri.extension.begin = p - 1;
            request->uri_state = URI_S_EXTENSION;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_ABS_PATH_DDOT:
        switch (ch) {
        case ' ':
            ++request->uri.nddots;
            request->uri.abs_path.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '.':
            request->uri.extension.begin = p;
            request->uri_state = URI_S_EXTENSION;
            break;
        case '?':
            ++request->uri.nddots;
            request->uri.abs_path.end = p;
            
            request->uri.query.begin = p;
            request->uri_state = URI_S_QUERY;
            break;
        case '/':
            ++request->uri.nddots;
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            request->uri.extension.begin = p;
            request->uri_state = URI_S_EXTENSION;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
      
    case URI_S_ABS_PATH_ENTRY:
        switch (ch) {
        case ' ':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            request->uri.abs_path.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '.':
            request->uri.extension.begin = p;
            request->uri_state = URI_S_EXTENSION;
            break;
        case '?':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            request->uri.abs_path.end = p;
            
            request->uri.query.begin = p;
            request->uri_state = URI_S_QUERY;
            break;
        case '/':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            break;
        default:
            return ERR_INVALID_URI;
        }
    
    case URI_S_EXTENSION:
        switch (ch) {
        case ' ':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            ++request->uri.extension.begin;
            request->uri.extension.end = p;
            
            request->uri.abs_path.end = p;
            request->uri_state = URI_S_BEGIN;
            break;
        case '.':
            request->uri.extension.begin = p;
            break;
        case '/':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            request->uri.extension.begin = NULL;
            request->uri_state = URI_S_ABS_PATH_SLASH;
            break;
        case '?':
            if (request->uri.nentries >= request->uri.nddots)
                ++request->uri.nentries;
            ++request->uri.extension.begin;
            request->uri.extension.end = p;
            
            request->uri.query.begin = p;
            request->uri_state = URI_S_QUERY;
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
            
            request->uri_state = URI_S_BEGIN;
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

int parse_header_line(request_t* request)
{
    buffer_t* buffer = &request->buffer;
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
                request->header_name.begin = p;
                request->state = HL_S_NAME;
                break;
            case '-':
                *p = *p - '-' + '_';
                ch = *p;
            case '0' ... '9':
            case 'a' ... 'z':
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
                break;
            case '-':
                // Convert '-' to '_'
                *p = *p - '-' + '_';
                ch = *p;

                // Fall through
            case '0' ... '9':
            case 'a' ... 'z':

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
    request->state = HL_S_BEGIN;
    
    return request->header_name.begin == NULL ? EMPTY_LINE: OK;
}

int parse_accept_value(request_t* request)
{
/*
    string_t* val = &request->header_value;
    list* accept_list = &requets->accept_list;
    
    for (char* p = val->begin; p < val->end;) {
        while (*p == ' ' && p != val->end) {
            ++p;
        }
        if (p == val->end)
            return OK;
        list_node_t* type_node = list_alloc(accept_list);
        accept_type_t* accept_type = &type_node->data;
        accept_type->type.begin = p;
        while (*p != '/' && p != val->end) {
            ++p;
        }
        if (p == val->end)
            return ERR_INVALID_HEADER;
        accept_type->type.end = p;
        
        ++p;
        accept_type->subtype.begin = p;
        while (*p != ';' && *p != ',' && p != val->end) {
            ++p;
        }
        //if (p == val->end)
        //    return ERR_INVALID_HEADER;
        accept_type->subtype.end = p;
        if (*p == ',')
            goto media_range;
        
    param:
        ++p;
        while (*p == ' ' && p != val->end) {
            ++p;
        }
        // Other params are ignored
        if (p[0] == 'q' && p[1] == '=' && p != val->end) {
            accept_type->q = atof(p + 2);
        } else {
            accept_type = 1;
        }
        
        while (*p != ';' && *p != ',' && p != val->end) {
            ++p;
        }
        if (*p == ';')
            goto param;
    media_range:
        list_insert(accept_list, list_tail(accept_list), type_node);
    }
*/    
    return OK;
}

int parse_request_body_chunked(request_t* request)
{
    assert(0);
    buffer_t* buffer = &request->buffer;
    char* p;
    for (p = buffer->begin; p < buffer->end; p++) {
        //char ch = *p;
        switch (request->state) {
            
        }
    }
    
    return OK;
}

int parse_request_body_no_encoding(request_t* request)
{
    buffer_t* buffer = &request->buffer;
    request->content_length -= buffer_size(buffer);
    buffer_clear(buffer);
    if (request->content_length == 0)
        return OK;
    else if (request->content_length > 0)
        return AGAIN;
    return ERR_INVALID_REQUEST;
}
