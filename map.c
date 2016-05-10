#include "map.h"
#include "connection.h"
#include <stddef.h>
#include "string.h"


/*
 * Headers map
 */
 
typedef struct header_node header_slot_t;
struct header_node {
    int offset;
    string_t header;
    header_slot_t* next;
};

/*
 * The header_map mapping from hash value of the header 
 * to the offset the header in request or response.
 * Can't insert more key into the map once it is constructed.
 * Chain used to solve collision, the new slot is 'allocated'
 * from the end of the map.
 * As the number of the header is less than 50, double size map 
 * is enough for any collisions. 
 */
#define MAP_SIZE    (131)
static header_slot_t header_map[2 * MAP_SIZE];
static header_slot_t* header_cur;

static void header_insert(hash_t hash, string_t header, int offset);
static hash_t string_hash(string_t* str);

static void header_insert(hash_t hash, string_t header, int offset)
{
    header_slot_t* slot = &header_map[hash % MAP_SIZE];
    if (slot->offset == -1) {
        slot->header = header;
        slot->offset = offset;
        return;
    }
    header_slot_t* new_slot = header_cur++;
    new_slot->header = header;
    new_slot->offset = offset;
    new_slot->next = slot->next;
    slot->next = new_slot;
}

static hash_t string_hash(string_t* str)
{
    hash_t hash = 0;
    for (char* p = str->begin; p < str->end; p++)
        hash = header_hash(hash, *p);
    return hash;
}

void register_request_headers(void)
{
    for (int i = 0; i < 2 * MAP_SIZE; i++) {
        header_slot_t* slot = &header_map[i];
        slot->offset = -1;
        string_init(&slot->header);
        slot->next = NULL;
    }
    header_cur = &header_map[MAP_SIZE];

    // Init headers
#   define PUT_HEADER(headers, member)                  \
        {                                               \
            string_t header = string_setto(#member);    \
            header_insert(string_hash(&header),         \
                    header, offsetof(headers, member)); \
        }

    PUT_HEADER(request_headers_t, cache_control);
    PUT_HEADER(request_headers_t, connection);
    PUT_HEADER(request_headers_t, date);
    PUT_HEADER(request_headers_t, pragma);
    PUT_HEADER(request_headers_t, trailer);
    PUT_HEADER(request_headers_t, transfer_encoding);
    PUT_HEADER(request_headers_t, upgrade);
    PUT_HEADER(request_headers_t, via);
    PUT_HEADER(request_headers_t, warning);
    PUT_HEADER(request_headers_t, allow);
    PUT_HEADER(request_headers_t, content_encoding);
    PUT_HEADER(request_headers_t, content_language);
    PUT_HEADER(request_headers_t, content_length);
    PUT_HEADER(request_headers_t, content_location);
    PUT_HEADER(request_headers_t, content_md5);
    PUT_HEADER(request_headers_t, content_range);
    PUT_HEADER(request_headers_t, content_type);
    PUT_HEADER(request_headers_t, expires);
    PUT_HEADER(request_headers_t, last_modified);
    
    PUT_HEADER(request_headers_t, accept);
    PUT_HEADER(request_headers_t, accept_charset);
    PUT_HEADER(request_headers_t, accept_encoding);
    PUT_HEADER(request_headers_t, authorization);
    PUT_HEADER(request_headers_t, expect);
    PUT_HEADER(request_headers_t, from);
    PUT_HEADER(request_headers_t, host);
    PUT_HEADER(request_headers_t, if_match);
    PUT_HEADER(request_headers_t, if_modified_since);
    PUT_HEADER(request_headers_t, if_none_match);
    PUT_HEADER(request_headers_t, if_range);
    PUT_HEADER(request_headers_t, if_unmodified_since);
    PUT_HEADER(request_headers_t, max_forwards);
    PUT_HEADER(request_headers_t, proxy_authorization);
    PUT_HEADER(request_headers_t, range);
    PUT_HEADER(request_headers_t, referer);
    PUT_HEADER(request_headers_t, te);
    PUT_HEADER(request_headers_t, user_agent);

#   undef PUT_HEADER
}

int header_offset(hash_t hash, string_t header)
{
    header_slot_t* slot = &header_map[hash % MAP_SIZE];
    if (slot->offset == -1)
        return slot->offset;
    while (slot && string_cmp(&slot->header, &header) != 0) {
        slot = slot->next;
    }
    if (slot == NULL)
        return -1;
    return slot->offset;
}
