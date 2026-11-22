/* OBS-MCP typed configuration. MIT licensed. */
#ifndef OBS_MCP_CONFIG_H
#define OBS_MCP_CONFIG_H

#include <stddef.h>

#define OBSMCP_MAX_PATH 512
#define OBSMCP_MAX_NAME 64
#define OBSMCP_DEFAULT_MAX_REQUEST (1024u * 1024u)

typedef struct {
    char name[OBSMCP_MAX_NAME];
    char obs_exe[OBSMCP_MAX_PATH];
    char working_dir[OBSMCP_MAX_PATH];
    char profile[OBSMCP_MAX_NAME];
    char collection[OBSMCP_MAX_NAME];
    char ws_host[OBSMCP_MAX_NAME];
    unsigned ws_port;
    char log_dir[OBSMCP_MAX_PATH];
    unsigned long long pid;
} obsmcp_instance_t;

typedef struct {
    obsmcp_instance_t sender;
    obsmcp_instance_t receiver;
    char harness_exe[OBSMCP_MAX_PATH];
    char harness_config[OBSMCP_MAX_PATH];
    char capture_config[OBSMCP_MAX_PATH];
    size_t max_request_bytes;
    unsigned long long capture_pid;
} obsmcp_config_t;

void obsmcp_config_init(obsmcp_config_t *config);
int obsmcp_config_load(obsmcp_config_t *config, const char *path);
const obsmcp_instance_t *obsmcp_instance(const obsmcp_config_t *config, const char *name);
obsmcp_instance_t *obsmcp_instance_mut(obsmcp_config_t *config, const char *name);

#endif
