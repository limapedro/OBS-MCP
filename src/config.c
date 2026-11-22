/* OBS-MCP typed TOML config loader. MIT licensed. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text)
{
    char *end;
    while (*text && (unsigned char)*text <= ' ')
        ++text;
    end = text + strlen(text);
    while (end > text && (unsigned char)end[-1] <= ' ')
        --end;
    *end = '\0';
    return text;
}

static void set_str(char *dst, size_t cap, const char *value)
{
    snprintf(dst, cap, "%s", value);
}

void obsmcp_config_init(obsmcp_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->sender.name[0] = 0;
    set_str(config->sender.name, sizeof(config->sender.name), "sender");
    set_str(config->sender.obs_exe, sizeof(config->sender.obs_exe),
            "third_party/obs-portable/sender/bin/64bit/obs64.exe");
    set_str(config->sender.working_dir, sizeof(config->sender.working_dir),
            "third_party/obs-portable/sender/bin/64bit");
    set_str(config->sender.profile, sizeof(config->sender.profile), "Wormhole");
    set_str(config->sender.collection, sizeof(config->sender.collection), "WormholeSender");
    set_str(config->sender.ws_host, sizeof(config->sender.ws_host), "127.0.0.1");
    config->sender.ws_port = 4455;
    set_str(config->sender.log_dir, sizeof(config->sender.log_dir),
            "third_party/obs-portable/sender/config/obs-studio/logs");

    set_str(config->receiver.name, sizeof(config->receiver.name), "receiver");
    set_str(config->receiver.obs_exe, sizeof(config->receiver.obs_exe),
            "third_party/obs-portable/receiver/bin/64bit/obs64.exe");
    set_str(config->receiver.working_dir, sizeof(config->receiver.working_dir),
            "third_party/obs-portable/receiver/bin/64bit");
    set_str(config->receiver.profile, sizeof(config->receiver.profile), "Wormhole");
    set_str(config->receiver.collection, sizeof(config->receiver.collection), "WormholeReceiver");
    set_str(config->receiver.ws_host, sizeof(config->receiver.ws_host), "127.0.0.1");
    config->receiver.ws_port = 4456;
    set_str(config->receiver.log_dir, sizeof(config->receiver.log_dir),
            "third_party/obs-portable/receiver/config/obs-studio/logs");

    set_str(config->harness_exe, sizeof(config->harness_exe), "build/wormhole-harness.exe");
    set_str(config->harness_config, sizeof(config->harness_config), "harness/config/example.toml");
    set_str(config->capture_config, sizeof(config->capture_config), "harness/config/obs-proxy.toml");
    config->max_request_bytes = OBSMCP_DEFAULT_MAX_REQUEST;
}

static void apply(obsmcp_config_t *config, const char *section, const char *key, const char *raw)
{
    char value[OBSMCP_MAX_PATH];
    obsmcp_instance_t *inst = NULL;

    if (strcmp(section, "sender") == 0)
        inst = &config->sender;
    else if (strcmp(section, "receiver") == 0)
        inst = &config->receiver;

    {
        char *v = value;
        snprintf(value, sizeof(value), "%s", raw);
        v = trim(v);
        {
            char *hash = strchr(v, '#');
            if (hash)
                *hash = '\0';
        }
        v = trim(v);
        {
            size_t n = strlen(v);
            if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
                v[n - 1] = '\0';
                memmove(v, v + 1, n - 1);
            }
        }
        memmove(value, v, strlen(v) + 1);
    }

    if (inst != NULL) {
        if (strcmp(key, "name") == 0) set_str(inst->name, sizeof(inst->name), value);
        else if (strcmp(key, "obs_exe") == 0) set_str(inst->obs_exe, sizeof(inst->obs_exe), value);
        else if (strcmp(key, "working_dir") == 0) set_str(inst->working_dir, sizeof(inst->working_dir), value);
        else if (strcmp(key, "profile") == 0) set_str(inst->profile, sizeof(inst->profile), value);
        else if (strcmp(key, "collection") == 0) set_str(inst->collection, sizeof(inst->collection), value);
        else if (strcmp(key, "ws_host") == 0) set_str(inst->ws_host, sizeof(inst->ws_host), value);
        else if (strcmp(key, "ws_port") == 0) inst->ws_port = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "log_dir") == 0) set_str(inst->log_dir, sizeof(inst->log_dir), value);
        return;
    }

    if (strcmp(section, "harness") == 0) {
        if (strcmp(key, "exe") == 0) set_str(config->harness_exe, sizeof(config->harness_exe), value);
        else if (strcmp(key, "config") == 0) set_str(config->harness_config, sizeof(config->harness_config), value);
        else if (strcmp(key, "capture_config") == 0) set_str(config->capture_config, sizeof(config->capture_config), value);
    } else if (strcmp(section, "general") == 0) {
        if (strcmp(key, "max_request_bytes") == 0)
            config->max_request_bytes = (size_t)strtoull(value, NULL, 10);
    }
}

int obsmcp_config_load(obsmcp_config_t *config, const char *path)
{
    FILE *file = fopen(path, "r");
    char line[2048];
    char section[64] = "";

    if (!file) {
        fprintf(stderr, "obs-mcp: cannot open config %s\n", path);
        return 1;
    }
    {
        unsigned char bom[3] = {0, 0, 0};
        if (fread(bom, 1, 3, file) == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
            /* skip BOM */
        } else {
            rewind(file);
        }
    }
    while (fgets(line, sizeof(line), file)) {
        char *text = trim(line);
        char *eq;
        char key[64];
        if (!*text || *text == '#')
            continue;
        if (*text == '[') {
            char *close = strchr(text, ']');
            if (!close) continue;
            *close = '\0';
            set_str(section, sizeof(section), text + 1);
            continue;
        }
        eq = strchr(text, '=');
        if (!eq) continue;
        *eq = '\0';
        set_str(key, sizeof(key), trim(text));
        apply(config, section, key, eq + 1);
    }
    fclose(file);
    return 0;
}

const obsmcp_instance_t *obsmcp_instance(const obsmcp_config_t *config, const char *name)
{
    if (strcmp(name, config->sender.name) == 0) return &config->sender;
    if (strcmp(name, config->receiver.name) == 0) return &config->receiver;
    return NULL;
}

obsmcp_instance_t *obsmcp_instance_mut(obsmcp_config_t *config, const char *name)
{
    if (strcmp(name, config->sender.name) == 0) return &config->sender;
    if (strcmp(name, config->receiver.name) == 0) return &config->receiver;
    return NULL;
}
