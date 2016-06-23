#include "fcgi.h"

typedef struct {
    unsigned char version;
    unsigned char type;
    unsigned char request_id_b1;
    unsigned char request_id_b0;
    unsigned char content_length_b1;
    unsigned char content_length_b0;
    unsigned char padding_length;
    unsigned char reserved;
} fcgi_header_t;


