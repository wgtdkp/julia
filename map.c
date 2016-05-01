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

void header_init(void)
{
    for (int i = 0; i < 2 * MAP_SIZE; i++) {
        header_slot_t* slot = &header_map[i];
        slot->offset = -1;
        string_init(&slot->header);
        slot->next = NULL;
    }
    header_cur = &header_map[MAP_SIZE];
    
    // Init headers
#   define PUT_HEADER(inout, member)                        \
        {                                                   \
            string_t header = string_setto(#member);        \
            header_insert(string_hash(&header),             \
            header, offsetof(inout, member));               \
        }
    PUT_HEADER(headers_in_t, cache_control);
    PUT_HEADER(headers_in_t, connection);
    PUT_HEADER(headers_in_t, date);
    PUT_HEADER(headers_in_t, pragma);
    PUT_HEADER(headers_in_t, trailer);
    PUT_HEADER(headers_in_t, transfer_encoding);
    PUT_HEADER(headers_in_t, upgrade);
    PUT_HEADER(headers_in_t, via);
    PUT_HEADER(headers_in_t, warning);
    PUT_HEADER(headers_in_t, allow);
    PUT_HEADER(headers_in_t, content_encoding);
    PUT_HEADER(headers_in_t, content_language);
    PUT_HEADER(headers_in_t, content_length);
    PUT_HEADER(headers_in_t, content_location);
    PUT_HEADER(headers_in_t, content_md5);
    PUT_HEADER(headers_in_t, content_range);
    PUT_HEADER(headers_in_t, content_type);
    PUT_HEADER(headers_in_t, expires);
    PUT_HEADER(headers_in_t, last_modified);
    
    PUT_HEADER(headers_in_t, accept);
    PUT_HEADER(headers_in_t, accept_charset);
    PUT_HEADER(headers_in_t, accept_encoding);
    PUT_HEADER(headers_in_t, authorization);
    PUT_HEADER(headers_in_t, expect);
    PUT_HEADER(headers_in_t, from);
    PUT_HEADER(headers_in_t, host);
    PUT_HEADER(headers_in_t, if_match);
    PUT_HEADER(headers_in_t, if_modified_since);
    PUT_HEADER(headers_in_t, if_none_match);
    PUT_HEADER(headers_in_t, if_range);
    PUT_HEADER(headers_in_t, if_unmodified_since);
    PUT_HEADER(headers_in_t, max_forwards);
    PUT_HEADER(headers_in_t, proxy_authorization);
    PUT_HEADER(headers_in_t, range);
    PUT_HEADER(headers_in_t, referer);
    PUT_HEADER(headers_in_t, te);
    PUT_HEADER(headers_in_t, user_agent);
    
    PUT_HEADER(headers_out_t, accept_ranges);
    PUT_HEADER(headers_out_t, age);
    PUT_HEADER(headers_out_t, etag);
    PUT_HEADER(headers_out_t, location);
    PUT_HEADER(headers_out_t, proxy_authenticate);
    PUT_HEADER(headers_out_t, retry_after);
    PUT_HEADER(headers_out_t, server);
    PUT_HEADER(headers_out_t, vary);
    PUT_HEADER(headers_out_t, www_authenticate);
}

/*
 * WARNING(wgtdkp): it is possible that, 
 * we receive a header in request
 * that can only be included in response.
 * Then, we incorrectly setup the request header's value! 
 */
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
