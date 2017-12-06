/* 
 * Based on nginx ngx_http_parse.c
 */

#include "server.h"


#define STR2_EQ(p, q)   ((p)[0] == (q)[0] && (p)[1] == (q)[1])
#define STR3_EQ(p, q)   (STR2_EQ(p, q) && (p)[2] == (q)[2])
#define STR4_EQ(p, q)   (STR2_EQ(p, q) && STR2_EQ(p + 2, q + 2))
#define STR5_EQ(p, q)   (STR2_EQ(p, q) && STR3_EQ(p + 2, q + 2))
#define STR6_EQ(p, q)   (STR3_EQ(p, q) && STR3_EQ(p + 3, q + 3))
#define STR7_EQ(p, q)   (STR3_EQ(p, q) && STR4_EQ(p + 3, q + 3))

#define PARSE_ERR_ON(cond, err) {   \
    if (cond) {                     \
        return (err);               \
    }                               \
};

static vector_t header_values;
static vector_t header_value_params;

static int parse_uri(uri_t* r, char* p);
static int parse_method(char* begin, char* end);
static void split_header_value(string_t* val, char split, vector_t* vec);

void parse_init(void) {
    vector_init(&header_values, sizeof(string_t), 0);
    vector_init(&header_value_params, sizeof(string_t), 0);
}

static inline int parse_method(char* begin, char* end) {
    // End of method
    int len = end - begin;
    switch (len) {
    case 3:
        if (STR3_EQ(begin, "GET")) {
            return M_GET;
        } else if(STR3_EQ(begin, "PUT")) {
            return M_PUT;
        } else {
            return ERR_INVALID_METHOD;
        }
        break;
    case 4:
        if (STR4_EQ(begin, "POST")) {
            return M_POST;
        } else if (STR4_EQ(begin, "HEAD")) {
            return M_HEAD;
        } else {
            return ERR_INVALID_METHOD;
        }
        break;
    case 5:
        if (STR5_EQ(begin, "TRACE")) {
            return M_TRACE;
        } else {
            return ERR_INVALID_METHOD;
        }
        break;
    case 6:
        if (STR6_EQ(begin, "DELETE")) {
            return M_DELETE;
        } else {
            return ERR_INVALID_METHOD;
        }
        break;
    case 7:
        if (STR7_EQ(begin, "CONNECT")) {
            return M_CONNECT;
        } else if (STR7_EQ(begin, "OPTIONS")) {
            return M_OPTIONS;
        } else {
            return ERR_INVALID_METHOD;
        }
        break;
    default:
        return ERR_INVALID_METHOD;
    }
    return ERR_INVALID_METHOD;   
}

int parse_request_line(request_t* r) {
    buffer_t* b = &r->rb;
    int uri_err;
    char* p;
    for (p = b->begin; p < b->end; ++p) {
        char ch = *p;
        
        switch (r->state) {
        case RL_S_BEGIN:
            switch (ch) {
            case 'A' ... 'Z':
                r->request_line.data = p;
                r->state = RL_S_METHOD;
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
                r->method = parse_method(r->request_line.data, p);
                if (r->method == ERR_INVALID_METHOD) {
                    return r->method;
                }
                r->state = RL_S_SP_BEFORE_URI;
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
                r->uri.state = URI_S_BEGIN;
                if ((uri_err = parse_uri(&r->uri, p)) != OK) {
                    return uri_err;
                }
                r->state = RL_S_URI;
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
                r->state = RL_S_SP_BEFORE_VERSION;
                // Fall through
            default: 
                if ((uri_err = parse_uri(&r->uri, p)) != OK) {
                    return uri_err;
                }
                if (ch == ' ') {
                    if (r->uri.nentries < r->uri.nddots) {
                        r->uri.abs_path.len = 1;        
                    }
                }
                break;
            }
            break;

        case RL_S_SP_BEFORE_VERSION:
            switch (ch) {
            case ' ':
                break;
            case 'H':
                r->state = RL_S_HTTP_H;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        // Is "HTTP/major.minor" case-sensitive?
        case RL_S_HTTP_H:
            PARSE_ERR_ON(ch != 'T', ERR_INVALID_REQUEST);
            r->state = RL_S_HTTP_HT;
            break;

        case RL_S_HTTP_HT:
            PARSE_ERR_ON(ch != 'T', ERR_INVALID_REQUEST);
            r->state = RL_S_HTTP_HTT;
            break;

        case RL_S_HTTP_HTT:
            PARSE_ERR_ON(ch != 'P', ERR_INVALID_REQUEST);
            r->state = RL_S_HTTP_HTTP;
            break;

        case RL_S_HTTP_HTTP:
            PARSE_ERR_ON(ch != '/', ERR_INVALID_REQUEST);
            r->state = RL_S_HTTP_VERSION_SLASH;
            break;

        case RL_S_HTTP_VERSION_SLASH:
            switch (ch) {
            case '0' ... '9':
                r->version.major =
                        r->version.major * 10 + ch - '0';
                r->state = RL_S_HTTP_VERSION_MAJOR;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_MAJOR:
            switch (ch) {
            case '0' ... '9':
                r->version.major = r->version.major * 10 + ch - '0';
                PARSE_ERR_ON(r->version.major > 999, ERR_INVALID_VERSION);
                break;
            case '.':
                r->state = RL_S_HTTP_VERSION_DOT;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_DOT:
            switch (ch) {
            case '0' ... '9':
                r->version.minor =
                        r->version.minor * 10 + ch - '0';
                r->state = RL_S_HTTP_VERSION_MINOR;
                break;
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_HTTP_VERSION_MINOR:
            switch (ch) {
            case '0' ... '9':
                r->version.minor =
                        r->version.minor * 10 + ch - '0';
                PARSE_ERR_ON(r->version.minor > 999, ERR_INVALID_VERSION);
                break;
            case ' ':
                r->state = RL_S_SP_AFTER_VERSION;
                break;
            case '\r':
                r->state = RL_S_ALMOST_DONE;
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
                r->state = RL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                return ERR_INVALID_REQUEST;
            }
            break;

        case RL_S_ALMOST_DONE:
            PARSE_ERR_ON(ch != '\n', ERR_INVALID_REQUEST);
            goto done;

        default:
            printf("r->state: %d\n", r->state);
            assert(false);
        }
    }

    b->begin = b->end;
    return AGAIN;

done:
    b->begin = p + 1;
    r->request_line.len = p - r->request_line.data;
    r->state = HL_S_BEGIN;
    return OK;
}

/*
 * Request-URI = [scheme ":" "//" host[":" port]][abs_path["?" query]]
 */
static int parse_uri(uri_t* uri, char* p) {
    char ch = *p;
    switch (uri->state) {
    case URI_S_BEGIN:
        switch (ch) {
        case '/':
            uri->abs_path.data = p;
            uri->state = URI_S_ABS_PATH_SLASH;
            break;
        case 'A' ... 'Z':
        case 'a' ... 'z':
            uri->scheme.data = p;
            uri->state = URI_S_SCHEME;
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
            uri->scheme.len = p - uri->scheme.data;
            uri->state = URI_S_SCHEME_COLON;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_COLON:
        switch (ch) {
        case '/':
            uri->state = URI_S_SCHEME_SLASH;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_SCHEME_SLASH:
        switch (ch) {
        case '/':
            uri->state = URI_S_SCHEME_SLASH_SLASH;
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
            uri->host.data = p;
            uri->state = URI_S_HOST;
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
            uri->state = URI_S_PORT;
            break;
        case '/':
            uri->abs_path.data = p;
            uri->state = URI_S_ABS_PATH_SLASH;
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
            uri->abs_path.data = uri->host.data - 2;
            uri->abs_path.len = 1;

            ++uri->port.data;
            uri->port.len = p - uri->port.data;
            uri->state = URI_S_BEGIN;
            break;
        case '/':
            uri->abs_path.data = p;

            ++uri->port.data;
            uri->port.len = p - uri->port.data;
            uri->state = URI_S_ABS_PATH_SLASH;
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
            uri->abs_path.len = p - uri->abs_path.data;
            uri->state = URI_S_BEGIN;
            break;
        case '.':
            uri->state = URI_S_ABS_PATH_DOT;
            break;
        case '?':
            uri->abs_path.len = p - uri->abs_path.data;
            
            uri->query.data = p;
            uri->state = URI_S_QUERY;
            break;
        case '/':
            break;
        case ALLOWED_IN_ABS_PATH:
            uri->state = URI_S_ABS_PATH_ENTRY;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_ABS_PATH_DOT:
        switch (ch) {
        case ' ':
            uri->abs_path.len = p - uri->abs_path.data;
            uri->state = URI_S_BEGIN;
            break;
        case '.':
            uri->state = URI_S_ABS_PATH_DDOT;
            break;
        case '?':
            uri->abs_path.len = p - uri->abs_path.data;
            
            uri->query.data = p;
            uri->state = URI_S_QUERY;
            break;
        case '/':
            uri->state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            // Extension begins at the '.'
            uri->extension.data = p - 1;
            uri->state = URI_S_EXTENSION;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
    
    case URI_S_ABS_PATH_DDOT:
        switch (ch) {
        case ' ':
            ++uri->nddots;
            uri->abs_path.len = p - uri->abs_path.data;
            uri->state = URI_S_BEGIN;
            break;
        case '.':
            uri->extension.data = p;
            uri->state = URI_S_EXTENSION;
            break;
        case '?':
            ++uri->nddots;
            uri->abs_path.len = p - uri->abs_path.data;
            
            uri->query.data = p;
            uri->state = URI_S_QUERY;
            break;
        case '/':
            ++uri->nddots;
            uri->state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            uri->extension.data = p;
            uri->state = URI_S_EXTENSION;
            break;
        default:
            return ERR_INVALID_URI;
        }
        break;
      
    case URI_S_ABS_PATH_ENTRY:
        switch (ch) {
        case ' ':
            if (uri->nentries >= uri->nddots)
                ++uri->nentries;
            uri->abs_path.len = p - uri->abs_path.data;
            uri->state = URI_S_BEGIN;
            break;
        case '.':
            uri->extension.data = p;
            uri->state = URI_S_EXTENSION;
            break;
        case '?':
            if (uri->nentries >= uri->nddots)
                ++uri->nentries;
            uri->abs_path.len = p - uri->abs_path.data;
            
            uri->query.data = p;
            uri->state = URI_S_QUERY;
            break;
        case '/':
            if (uri->nentries >= uri->nddots)
                ++uri->nentries;
            uri->state = URI_S_ABS_PATH_SLASH;
            break;
        case ALLOWED_IN_ABS_PATH:
            break;
        default:
            return ERR_INVALID_URI;
        }
    
    case URI_S_EXTENSION:
        switch (ch) {
        case ' ':
            if (uri->nentries >= uri->nddots) {
                ++uri->nentries;
            }
            ++uri->extension.data;
            uri->extension.len = p - uri->extension.data;
            
            uri->abs_path.len = p - uri->abs_path.data;
            uri->state = URI_S_BEGIN;
            break;
        case '.':
            uri->extension.data = p;
            break;
        case '/':
            if (uri->nentries >= uri->nddots) {
                ++uri->nentries;
            }
            uri->extension.data = NULL;
            uri->state = URI_S_ABS_PATH_SLASH;
            break;
        case '?':
            if (uri->nentries >= uri->nddots) {
                ++uri->nentries;
            }
            ++uri->extension.data;
            uri->extension.len = p - uri->extension.data;
            
            uri->query.data = p;
            uri->state = URI_S_QUERY;
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
            ++uri->query.data;
            uri->query.len = p - uri->query.data;
            
            uri->state = URI_S_BEGIN;
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
        printf("uri->state: %d\n", uri->state);
        assert(false);
    }
    
    return OK;
}

int parse_header_line(request_t* r) {
    buffer_t* b = &r->rb;
    char* p;
    for (p = b->begin; p < b->end; ++p) {
        char ch = *p;
        switch (r->state) {
        case HL_S_BEGIN:
            // Reset states
            string_init(&r->header_name);
            string_init(&r->header_value);
            switch(ch) {
            case 'A' ... 'Z':
                *p = *p - 'A' + 'a';
                ch = *p;
                r->header_name.data = p;
                r->state = HL_S_NAME;
                break;
            case '-':
                *p = *p - '-' + '_';
                ch = *p;
            case '0' ... '9':
            case 'a' ... 'z':
                r->header_name.data = p;
                r->state = HL_S_NAME;
                break;
            case '\r':
                r->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                goto header_done;
            default:
                r->state = HL_S_IGNORE;
                break;
            }
            break;
        
        case HL_S_IGNORE:
            switch (ch) {
            case '\n':
                r->state = HL_S_BEGIN;
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
                r->header_name.len = p - r->header_name.data;
                r->state = HL_S_COLON;
                break;
            case '\r':
                r->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
            default:
                r->state = HL_S_IGNORE;
                break;
            }
            break;

        case HL_S_SP_BEFORE_VALUE: 
        case HL_S_COLON:
            switch (ch) {
            case ' ':
                r->state = HL_S_SP_BEFORE_VALUE;
                break;
            case '\r':
                r->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                goto header_done;
            default:
                r->header_value.data = p;
                r->state = HL_S_VALUE;
                break;
            }
            break;

        case HL_S_VALUE:
            switch (ch) {
            case ' ':
                r->header_value.len = p - r->header_value.data;
                r->state = HL_S_SP_AFTER_VALUE;
                break;
            case '\r':
                r->header_value.len = p - r->header_value.data;
                r->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                r->header_value.len = p - r->header_value.data;
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
                r->state = HL_S_ALMOST_DONE;
                break;
            case '\n':
                // Single '\n' is also accepted
                goto header_done;
            default:
                r->state = HL_S_VALUE;
                break;
            }
            break;

        case HL_S_ALMOST_DONE:
            switch (ch) {
            case '\n':
                goto header_done;
            default:
                r->state = HL_S_IGNORE;
                break;
            }
            break;

        default:
            printf("r->state: %d\n", r->state);
            assert(false);
        }
    }
    
    b->begin = b->end;
    return AGAIN;

header_done:
    b->begin = p + 1;
    r->state = HL_S_BEGIN;
    
    return r->header_name.data == NULL ? EMPTY_LINE: OK;
}

void parse_header_host(request_t* r) {
    string_t* host = &r->headers.host;
    char* semicolon = NULL;
    for (int i = 0; i < host->len; ++i) {
        if (host->data[i] == ':') {
            semicolon = &host->data[i];
        }
    }
    
    if (semicolon == NULL) {
        r->host = *host;
        r->port = 80;
    } else {
        r->host = (string_t){host->data, semicolon - host->data};
        r->port = atoi(semicolon + 1);
    }
}

/*
 * Split *( *LWS element *( *LWS "," *LWS element ))
 * LWS surround element were trimmed.
 */
static void split_header_value(string_t* val, char split, vector_t* vec) {
    vector_resize(vec, 0); // Clear previous data

    string_t* cur = NULL;
    char* end = val->data + val->len;
    for (char* p = val->data; p < end; ++p) {
        if (*p == ' ') {
            if (cur != NULL) {
                cur->len = p - cur->data;
            }
        } else if (*p == split) {
            if (cur != NULL) {
                if (cur->len == 0) {
                    cur->len = p - cur->data;
                }
                cur = NULL;
            }
        } else if (cur == NULL) {
            cur = vector_push(vec);
            cur->data = p;
            cur->len = 0;
        } else {
            cur->len = 0;
        }
    }
    if (cur != NULL) {
        cur->len = end - cur->data;
    }
}

int parse_header_accept(request_t* r) {
    string_t* val = &r->header_value;
    list_t* accept_list = &r->accepts;
    
    split_header_value(val, ',', &header_values);
    for (int i = 0; i < header_values.size; ++i) {
        string_t* val = vector_at(&header_values, i);

        split_header_value(val, ';', &header_value_params);
        if (header_value_params.size == 0) {
            continue;   // Bad format
        }
        
        string_t* type = vector_at(&header_value_params, 0);
        char* slash = string_find(type, '/');
        if (slash == NULL) {
            continue;   // Bad format
        }
        
        list_node_t* type_node = list_alloc(accept_list);
        accept_type_t* accept_type = (accept_type_t*)&type_node->data;
        accept_type->type.data = type->data;
        accept_type->type.len = slash - type->data;
        accept_type->subtype.data = slash + 1;
        accept_type->subtype.len = type->len - accept_type->type.len - 1;
        
        accept_type->q = 1.0f;
        for (int j = 1; j < header_value_params.size; ++j) {
            string_t* param = vector_at(&header_value_params, j);
            if (param->data[0] == 'q' && param->data[1] == '=') {
                accept_type->q = atof(&param->data[2]);
            }
        }
        list_insert(accept_list, list_tail(accept_list), type_node);
    }
    return OK;
}

// TODO(wgtdkp):
int parse_request_body_chunked(request_t* r) {
    assert(false);
    buffer_t* b = &r->rb;
    for (char* p = b->begin; p < b->end; ++p) {
        //char ch = *p;
        switch (r->state) {
            
        }
    }
    return OK;
}

int parse_request_body_identity(request_t* r) {
    buffer_t* b = &r->rb;    
    if (r->content_length <= 0) {
        return OK;
    }

    // Error here
    r->body_received += buffer_size(b);
    if (r->body_received >= r->content_length) {
        // Consume the body
        b->begin += buffer_size(b) - (r->body_received - r->content_length);
        return OK;
    }
    // Consume the body
    b->begin += buffer_size(b);
    return AGAIN;
}
