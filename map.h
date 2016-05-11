#ifndef _JULIA_MAP_H_
#define _JULIA_MAP_H_

#include "string.h"


typedef unsigned int hash_t;

void header_map_init(void);
void mime_map_init(void);
int header_offset(string_t header);
string_t mime_type(string_t extension);



#endif
