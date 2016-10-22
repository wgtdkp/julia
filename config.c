#include "server.h"

#include "base/string.h"
#include "juson/juson.h"

config_t server_cfg;

#define CFG_ERR_ON(cond, msg)       \
if ((cond)) {                       \
    ju_error("config file: "msg);   \
    return ERROR;                   \
};

static inline const string_t juson_val2str(juson_value_t* val)
{
    return (const string_t){(char*)val->sval, val->len};
}

static void config_init(config_t* cfg)
{
    memset(cfg, 0, sizeof(*cfg));
}

int config_load(config_t* cfg, char* file_name)
{
    config_init(cfg);

    juson_doc_t json;
    cfg->text = juson_load(file_name);
    if (cfg->text == NULL) {
        ju_error("load file '%s' failed", file_name);
        return ERROR;
    }
    
    juson_value_t* root = juson_parse(&json, cfg->text);
    CFG_ERR_ON(root == NULL || root->t != JUSON_OBJECT, "bad format");
    
    juson_value_t* port_val = juson_object_get(root, "port");
    if (port_val == NULL || port_val->t != JUSON_INTEGER) {
        cfg->port = 80;
    } else {
        cfg->port = port_val->ival;
    }
    
    juson_value_t* root_val = juson_object_get(root, "root");
    CFG_ERR_ON(root_val == NULL || root_val->t != JUSON_STRING,
            "doc root not specified");
    cfg->root = juson_val2str(root_val);

    juson_value_t* locations_val = juson_object_get(root, "locations");
    CFG_ERR_ON(root_val == NULL || locations_val->t != JUSON_ARRAY ||
            locations_val->size == 0, "parse locations specification failed");
    
    vector_init(&cfg->locations, sizeof(location_t), locations_val->size);
    for (int i = 0; i < locations_val->size; ++i) {
        location_t* loc = vector_at(&cfg->locations, i);
        juson_value_t* loc_val = juson_array_get(locations_val, i);
        juson_value_t* path_val = juson_array_get(loc_val, 0);
        loc->path = juson_val2str(path_val);
        juson_value_t* loc_cfg_val = juson_array_get(loc_val, 1);
        juson_value_t* pass_val = juson_object_get(loc_cfg_val, "pass");
        if (pass_val != NULL) {
            loc->pass = true;
            loc->host = juson_val2str(juson_array_get(pass_val, 0));
            loc->port = juson_array_get(pass_val, 1)->ival;
            string_t protocol =
                    juson_val2str(juson_object_get(loc_cfg_val, "protocol"));
            if (string_eq(&protocol, &STRING("http")))
                loc->protocol = PROT_HTTP;
            else if (string_eq(&protocol, &STRING("fcgi")))
                loc->protocol = PROT_FCGI;
            else
                loc->protocol = PROT_UWSGI; // Default
        } else {
            loc->pass = false;
        }
    }

    juson_destroy(&json);
    return OK;
}

void config_destroy(config_t* cfg)
{
    free(cfg->text);
    vector_clear(&cfg->locations);
}
