#include "map.h"
#include "connection.h"
#include "string.h"

#include <stddef.h>



/*
 * Map from string_t to string_t or int
 */
typedef union {
    string_t sval;
    int ival; 
} map_val_t;

typedef string_t map_key_t;
typedef struct map_slot map_slot_t;
struct map_slot {
    string_t key;
    map_val_t val;
    map_slot_t* next;
};

typedef struct {
    int size;
    int max_size;
    map_slot_t* data;
    map_slot_t* cur;
} map_t;

/*
 * The header_map mapping from hash value of the header 
 * to the offset the header in request or response.
 * Can't insert more key into the map once it is constructed.
 * Chain used to solve collision, the new slot is 'allocated'
 * from the end of the map.
 * As the number of the header is less than 50, double size map 
 * is enough for any collisions. 
 */
#define HEADER_MAP_SIZE     (131)
#define MIME_MAP_SIZE       (131)
static map_slot_t header_map_data[2 * HEADER_MAP_SIZE];
static map_slot_t mime_map_data[2 * MIME_MAP_SIZE];

static map_t header_map = {
  .size = HEADER_MAP_SIZE,
  .max_size = 2 * HEADER_MAP_SIZE,
  .data = header_map_data,
  .cur = header_map_data + HEADER_MAP_SIZE  
};

static map_t mime_map = {
  .size = MIME_MAP_SIZE,
  .max_size = 2 * MIME_MAP_SIZE,
  .data = mime_map_data,
  .cur = mime_map_data + MIME_MAP_SIZE  
};

static char* mimes [][2] = {
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

static hash_t string_hash(string_t* str);

static map_slot_t* map_get(map_t* map, map_key_t key)
{
    hash_t hash = string_hash(&key);
    map_slot_t* slot = &map->data[hash % map->size];
    if (slot->key.begin == NULL)
        return NULL;
    while (slot && !string_eq(&slot->key, &key)) {
        slot = slot->next;
    }
    return slot;
}

static void map_put(map_t* map, string_t key, map_val_t val)
{
    hash_t hash = string_hash(&key);
    map_slot_t* slot = &map->data[hash % map->size];
    if (slot->key.begin == NULL) {
        slot->key = key;
        slot->val = val;
    }
    map_slot_t* new_slot = map->cur++;
    new_slot->key = key;
    new_slot->val = val;
    new_slot->next = slot->next;
    slot->next = new_slot;
}

void header_map_init(void)
{
    map_key_t key;
    map_val_t val;
    
#   define PUT_HEADER(headers, member)          \
        key = string_setto(#member);            \
        val.ival = offsetof(headers, member);   \
        map_put(&header_map, key, val);
    
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

void mime_map_init(void)
{   
    int len = sizeof(mimes) / sizeof(mimes[0]);
    for (int i = 0; i < len; i++) {
        map_key_t key = string_setto(mimes[i][0]);
        map_val_t val = {
          .sval = string_setto(mimes[i][1]),  
        };
        map_put(&mime_map, key, val);
    }
}

int header_offset(string_t header)
{
    map_slot_t* slot = map_get(&header_map, header);
    if (slot == NULL)
        return -1;
    return slot->val.ival;
}

string_t mime_type(string_t extension)
{
    map_slot_t* slot = map_get(&mime_map, extension);
    if (slot == NULL)
        return string_settol(NULL, 0);
    return slot->val.sval;
}

static hash_t string_hash(string_t* str)
{
    hash_t hash = 0;
    for (char* p = str->begin; p < str->end; p++)
        hash = (hash * 31) + *p;
    return hash;
}
