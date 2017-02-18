#ifndef _JULIA_MAP_H_
#define _JULIA_MAP_H_

#include "string.h"

typedef string_t mime_val_t;
typedef unsigned int hash_t;

/*
 * Map from string_t to string_t or int
 */
typedef struct {
    int offset;
    void* processor;
} header_val_t;

typedef union {
    mime_val_t mime;
    header_val_t header;
} map_val_t;

typedef string_t map_key_t;
typedef struct map_slot map_slot_t;
struct map_slot {
    string_t key;
    map_val_t val;
    map_slot_t* next;
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
typedef struct {
    int size;
    int max_size;
    map_slot_t* data;
    map_slot_t* cur;
} map_t;

map_slot_t* map_get(map_t* map, const map_key_t* key);
void map_put(map_t* map, const string_t* key, const map_val_t* val);

#endif
