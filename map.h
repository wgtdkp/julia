#ifndef _JULIA_MAP_H_
#define _JULIA_MAP_H_

#include "string.h"


typedef unsigned int hash_t;

void header_init(void);
int header_offset(hash_t hash, string_t header);
static inline hash_t header_hash(hash_t hash, int ch)
{
    return (hash * 31) + ch;
}

#endif
