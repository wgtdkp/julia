#ifndef _JULIA_CONFIG_H_
#define _JULIA_CONFIG_H_

#include "base/string.h"

#include <stdint.h>

typedef struct {
    string_t host;
    uint16_t port;
    
    string_t doc_root;
    
    char* data;
} config_t;

extern config_t server_cfg;

int config_load(config_t* cfg, char* file_name);

#endif
