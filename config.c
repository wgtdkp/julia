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

// Append null terminal at the end
static size_t move_string(char* des, string_t* src)
{
    memcpy(des, src->data, src->len);
    des[src->len] = '\0';
    src->data = des;
    return src->len + 1;
}

static int split_port(string_t* addr)
{
    const char* p = string_end(addr);
    while (p >= addr->data) {
        if (*p == ':') {
            addr->len = p - addr->data;
            return atoi(p + 1);
        }
        --p;
    }
    return -1;
}

int config_load(config_t* cfg, char* file_name)
{
    size_t len = 0;
    
    juson_doc_t json;
    char* text = juson_load(file_name);
    if (text == NULL) {
        ju_error("load file '%s' failed", file_name);
        return ERROR;
    }
    
    juson_value_t* root = juson_parse(&json, text);
    CFG_ERR_ON(root == NULL || root->t != JUSON_OBJECT, "bad format");
    
    juson_value_t* host_val = juson_object_get(root, "host");
    CFG_ERR_ON(host_val == NULL || host_val->t != JUSON_STRING,
            "host not specified");
    cfg->host = juson_val2str(host_val);
    len += cfg->host.len;
    
    juson_value_t* port_val = juson_object_get(root, "port");
    if (port_val == NULL || port_val->t != JUSON_INTEGER) {
        cfg->port = 80;
    } else {
        cfg->port = port_val->ival;
    }
    
    juson_value_t* doc_root_val = juson_object_get(root, "doc_root");
    CFG_ERR_ON(doc_root_val == NULL || doc_root_val->t != JUSON_STRING,
            "doc root not specified");
    cfg->doc_root = juson_val2str(doc_root_val);
    len += cfg->doc_root.len;
    
    juson_value_t* dynamic;
    juson_value_t* python;
    juson_value_t* addr;    
    if ((dynamic = juson_object_get(root, "dynamic")) &&
        (python = juson_object_get(dynamic, "python")) &&
        (addr = juson_object_get(python, "addr"))) {
        cfg->dynamic_addr = juson_val2str(addr);
        juson_value_t* socket = juson_object_get(python, "socket");
        CFG_ERR_ON(socket == NULL, "specify dynamic error, bad format");
        cfg->dynamic_unix_socket = strncmp("unix", socket->sval, 4) == 0;
        if (!cfg->dynamic_unix_socket) {
            int port = split_port(&cfg->dynamic_addr);
            CFG_ERR_ON(port < 0, "expect port for dynamic addr");
            cfg->dynamic_port = port;
        }
        len += cfg->dynamic_addr.len;
    } else {
        CFG_ERR_ON(1, "specify dynamic error, bad format");
    }
    
    cfg->data = malloc(len);
    CFG_ERR_ON(cfg->data == NULL, "no memory");
    
    len = 0;
    len += move_string(cfg->data + len, &cfg->host);
    len += move_string(cfg->data + len, &cfg->doc_root);
    len += move_string(cfg->data + len, &cfg->dynamic_addr);

    free(text);
    juson_destroy(&json);
    return OK;
}

void config_destroy(config_t* cfg)
{
    free(cfg->data);
}
