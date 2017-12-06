#include "server.h"

#include "base/string.h"
#include "juson/juson.h"

config_t server_cfg;

#define CFG_ERR_ON(cond, msg)       \
if ((cond)) {                       \
    ju_error("config file: "msg);   \
    return ERROR;                   \
};

static inline const string_t juson_val2str(juson_value_t* val) {
    string_t ret = {(char*)val->sval, val->len};
    ret.data[ret.len] = 0;
    return ret;
}

static void config_init(config_t* cfg) {
    memset(cfg, 0, sizeof(config_t));
}

int config_load(config_t* cfg) {
    config_init(cfg);
    const char* cfg_file = INSTALL_DIR "config.json";

    if (cfg->text) {
        free(cfg->text);
    }
    cfg->text = juson_load(cfg_file);
    if (cfg->text == NULL) {
        ju_error("load file '%s' failed", cfg_file);
        return ERROR;
    }

    juson_doc_t json;
    juson_value_t* root = juson_parse(&json, cfg->text);
    CFG_ERR_ON(root == NULL || root->t != JUSON_OBJECT, "bad format");
    
    juson_value_t* debug_val = juson_object_get(root, "debug");
    CFG_ERR_ON(debug_val == NULL || debug_val->t != JUSON_BOOL,
               "debug not specified or error type");
    cfg->debug = debug_val->bval;

    juson_value_t* daemon_val = juson_object_get(root, "daemon");
    CFG_ERR_ON(daemon_val == NULL || daemon_val->t != JUSON_BOOL,
               "daemon not specified or error type");
    cfg->daemon = daemon_val->bval;

    juson_value_t* worker_val = juson_object_get(root, "worker");
    CFG_ERR_ON(worker_val == NULL || worker_val->t != JUSON_INTEGER,
               "worker not specified or error type");
    vector_init(&cfg->workers, sizeof(int), worker_val->size);
    CFG_ERR_ON(cfg->workers.size > sysconf(_SC_NPROCESSORS_ONLN), 
               "wrokers specified greater than cpu cores");

    juson_value_t* timeout_val = juson_object_get(root, "timeout");
    CFG_ERR_ON(timeout_val == NULL || timeout_val->t != JUSON_INTEGER,
               "timeout not specified or error type");
    cfg->timeout = timeout_val->ival;

    juson_value_t* port_val = juson_object_get(root, "port");
    if (port_val == NULL || port_val->t != JUSON_INTEGER) {
        cfg->port = 80;
    } else {
        cfg->port = port_val->ival;
    }
    
    juson_value_t* root_val = juson_object_get(root, "root");
    CFG_ERR_ON(root_val == NULL || root_val->t != JUSON_STRING,
               "doc root not specified or error type");
    string_t path = juson_val2str(root_val);
    cfg->root_fd = open(path.data, O_RDONLY);
    CFG_ERR_ON(cfg->root_fd < 0, "open root failed");

    juson_value_t* locations_val = juson_object_get(root, "locations");
    CFG_ERR_ON(root_val == NULL ||
               locations_val->t != JUSON_ARRAY ||
               locations_val->size == 0,
               "parse locations specification failed");
    
    vector_init(&cfg->locations, sizeof(location_t), locations_val->size);
    for (int i = 0; i < locations_val->size; ++i) {
        location_t* loc = vector_at(&cfg->locations, i);
        juson_value_t* loc_val = juson_array_get(locations_val, i);
        juson_value_t* path_val = juson_array_get(loc_val, 0);
        loc->path = juson_val2str(path_val);
        juson_value_t* loc_cfg_val = juson_array_get(loc_val, 1);
        CFG_ERR_ON(loc_cfg_val == NULL || loc_cfg_val->t != JUSON_OBJECT,
                   "bad location specification");
        //juson_value_t* root_val = juson_object_get(loc_cfg_val, "root");
        //CFG_ERR_ON(roo_val == NULL || roo_val->t != JUSON_STRING,
        //           "root value should be string");
        //loc->root = juson_val2str(root_val);


        juson_value_t* pass_val = juson_object_get(loc_cfg_val, "pass");
        if (pass_val != NULL) {
            loc->pass = true;
            loc->host = juson_val2str(juson_array_get(pass_val, 0));
            loc->port = juson_array_get(pass_val, 1)->ival;
            string_t protocol =
                    juson_val2str(juson_object_get(loc_cfg_val, "protocol"));
            if (string_eq(&protocol, &STRING("http"))) {
                loc->protocol = PROT_HTTP;
            } else if (string_eq(&protocol, &STRING("fcgi"))) {
                loc->protocol = PROT_FCGI;
            } else {
                loc->protocol = PROT_UWSGI; // Default
            }
        } else {
            loc->pass = false;
        }
    }

    juson_destroy(&json);
    return OK;
}

void config_destroy(config_t* cfg) {
    free(cfg->text);
    vector_clear(&cfg->locations);
}
