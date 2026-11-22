/* OBS-MCP tool registry and dispatcher. MIT licensed. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/mcp.h"
#include "obs_mcp/obs_process.h"
#include "obs_mcp/obs_ws.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *obsmcp_strdup_local(const char *text)
{
    size_t n = strlen(text) + 1u;
    char *copy = (char *)malloc(n);
    if (copy) memcpy(copy, text, n);
    return copy;
}
#define strdup obsmcp_strdup_local

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

cJSON *obsmcp_text_result(const char *text)
{
    cJSON *content = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text ? text : "");
    cJSON_AddItemToArray(content, item);
    cJSON_AddItemToObject(root, "content", content);
    return root;
}

cJSON *obsmcp_error_result(const char *text)
{
    cJSON *root = obsmcp_text_result(text);
    cJSON_AddBoolToObject(root, "isError", 1);
    return root;
}

cJSON *obsmcp_tools_list(void)
{
    cJSON *tools = cJSON_CreateArray();
    const char *names[] = {
        "obs_list_instances", "obs_launch", "obs_terminate", "obs_cpu_stats",
        "obs_ws_call", "obs_log_tail", "harness_run", "capture_start", "capture_stop", "wait_ms"
    };
    const char *descs[] = {
        "List configured OBS instances and their running state",
        "Launch a configured portable OBS instance",
        "Terminate a configured OBS instance",
        "Sample CPU time, CPU percentage and working set for an instance",
        "Send an OBS WebSocket v5 request and return the response",
        "Return the last N lines of the newest OBS log",
        "Run a Wormhole C harness command with a typed TOML config",
        "Start the Wormhole capture proxy as a child process",
        "Stop the capture proxy child process",
        "Wait for a bounded number of milliseconds (100-10000)"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        cJSON *tool = cJSON_CreateObject();
        cJSON *schema = cJSON_CreateObject();
        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", names[i]);
        cJSON_AddStringToObject(tool, "description", descs[i]);
        cJSON_AddStringToObject(schema, "type", "object");
        cJSON_AddStringToObject(props, "name", "instance name");
        cJSON_AddStringToObject(props, "config", "path to TOML config");
        cJSON_AddStringToObject(props, "command", "harness subcommand");
        cJSON_AddItemToObject(schema, "properties", props);
        cJSON_AddItemToObject(tool, "inputSchema", schema);
        cJSON_AddItemToArray(tools, tool);
    }
    return tools;
}

static const char *arg_string(cJSON *args, const char *key, const char *fallback)
{
    cJSON *v = args ? cJSON_GetObjectItemCaseSensitive(args, key) : NULL;
    return cJSON_IsString(v) ? v->valuestring : fallback;
}

static int arg_int(cJSON *args, const char *key, int fallback, int lo, int hi)
{
    cJSON *v = args ? cJSON_GetObjectItemCaseSensitive(args, key) : NULL;
    int value = cJSON_IsNumber(v) ? v->valueint : fallback;
    if (value < lo) value = lo;
    if (value > hi) value = hi;
    return value;
}

static cJSON *instances_json(const obsmcp_config_t *config)
{
    cJSON *root = cJSON_CreateArray();
    const obsmcp_instance_t *list[2] = {&config->sender, &config->receiver};
    int i;
    for (i = 0; i < 2; ++i) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", list[i]->name);
        cJSON_AddBoolToObject(item, "running", obsmcp_is_running(list[i]));
        cJSON_AddNumberToObject(item, "pid", (double)list[i]->pid);
        cJSON_AddNumberToObject(item, "ws_port", list[i]->ws_port);
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

static int run_command(const char *cmd, char **out)
{
    char buffer[4096];
    size_t total = 0;
    size_t n;
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return 1;
    while ((n = fread(buffer + total, 1, sizeof(buffer) - total - 1, pipe)) > 0) {
        total += n;
        if (total >= sizeof(buffer) - 1) break;
    }
    buffer[total] = '\0';
    pclose(pipe);
    *out = (char *)malloc(total + 1);
    if (*out) strcpy(*out, buffer);
    return *out ? 0 : 1;
}

static int spawn_detached(const char *exe, const char *args, unsigned long long *pid, char *err, size_t err_cap)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe, args);
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        snprintf(err, err_cap, "CreateProcess failed: %lu", GetLastError());
        return 1;
    }
    *pid = (unsigned long long)pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
#else
    pid_t child = fork();
    if (child < 0) { snprintf(err, err_cap, "fork failed"); return 1; }
    if (child == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(1);
    }
    *pid = (unsigned long long)child;
    return 0;
#endif
}

static int stop_pid(unsigned long long pid)
{
    if (!pid) return 1;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) return 1;
    TerminateProcess(h, 0);
    CloseHandle(h);
    return 0;
#else
    return kill((pid_t)pid, SIGTERM);
#endif
}

cJSON *obsmcp_tool_call(obsmcp_config_t *config, const char *name, cJSON *args, int *is_error)
{
    char err[512] = "";
    char *text = NULL;
    cJSON *result = NULL;

    *is_error = 0;
    if (strcmp(name, "obs_list_instances") == 0) {
        cJSON *instances = instances_json(config);
        text = cJSON_PrintUnformatted(instances);
        cJSON_Delete(instances);
    } else if (strcmp(name, "obs_launch") == 0) {
        obsmcp_instance_t *inst = obsmcp_instance_mut(config, arg_string(args, "name", ""));
        if (!inst) {
            snprintf(err, sizeof(err), "unknown instance");
        } else if (obsmcp_launch(inst, err, sizeof(err))) {
            /* err filled */
        } else {
            snprintf(err, sizeof(err), "launched %s with pid %llu", inst->name, inst->pid);
        }
        text = strdup(err);
        if (err[0] && strncmp(err, "launched", 8) != 0) *is_error = 1;
    } else if (strcmp(name, "obs_terminate") == 0) {
        obsmcp_instance_t *inst = obsmcp_instance_mut(config, arg_string(args, "name", ""));
        if (!inst) snprintf(err, sizeof(err), "unknown instance");
        else if (obsmcp_terminate(inst, err, sizeof(err))) *is_error = 1;
        else snprintf(err, sizeof(err), "terminated %s", inst->name);
        text = strdup(err);
    } else if (strcmp(name, "obs_cpu_stats") == 0) {
        const obsmcp_instance_t *inst = obsmcp_instance(config, arg_string(args, "name", ""));
        unsigned ms = (unsigned)arg_int(args, "window_ms", 1000, 100, 10000);
        double cpu_s = 0, pct = 0, ws = 0;
        if (!inst) snprintf(err, sizeof(err), "unknown instance");
        else if (obsmcp_cpu_stats(inst, ms, &cpu_s, &pct, &ws, err, sizeof(err))) *is_error = 1;
        else snprintf(err, sizeof(err),
                      "{\"cpu_seconds\":%.3f,\"cpu_percent\":%.2f,\"working_set_mb\":%.1f,\"window_ms\":%u}",
                      cpu_s, pct, ws, ms);
        text = strdup(err);
    } else if (strcmp(name, "obs_ws_call") == 0) {
        const obsmcp_instance_t *inst = obsmcp_instance(config, arg_string(args, "name", ""));
        const char *request = arg_string(args, "request", "");
        char *response = NULL;
        if (!inst) snprintf(err, sizeof(err), "unknown instance");
        else if (obsmcp_ws_call(inst->ws_host, inst->ws_port, request, &response, err, sizeof(err))) *is_error = 1;
        else { text = response; response = NULL; }
        free(response);
        if (!text) text = strdup(err);
    } else if (strcmp(name, "obs_log_tail") == 0) {
        const obsmcp_instance_t *inst = obsmcp_instance(config, arg_string(args, "name", ""));
        unsigned lines = (unsigned)arg_int(args, "lines", 20, 1, 500);
        if (!inst) snprintf(err, sizeof(err), "unknown instance");
        else if (obsmcp_log_tail(inst, lines, &text, err, sizeof(err))) *is_error = 1;
        if (!text) text = strdup(err);
    } else if (strcmp(name, "harness_run") == 0) {
        char cmd[2048];
        const char *command = arg_string(args, "command", "check");
        const char *cfg = arg_string(args, "config", config->harness_config);
        snprintf(cmd, sizeof(cmd), "\"%s\" --config \"%s\" %s", config->harness_exe, cfg, command);
        if (run_command(cmd, &text)) {
            *is_error = 1;
            text = strdup("harness command failed");
        }
    } else if (strcmp(name, "capture_start") == 0) {
        char cmd_args[1024];
        snprintf(cmd_args, sizeof(cmd_args), "--config \"%s\" proxy", config->capture_config);
        if (spawn_detached(config->harness_exe, cmd_args, &config->capture_pid, err, sizeof(err))) *is_error = 1;
        else snprintf(err, sizeof(err), "capture proxy started with pid %llu", config->capture_pid);
        text = strdup(err);
    } else if (strcmp(name, "capture_stop") == 0) {
        if (stop_pid(config->capture_pid)) { *is_error = 1; text = strdup("no capture proxy running"); }
        else { config->capture_pid = 0; text = strdup("capture proxy stopped"); }
    } else if (strcmp(name, "wait_ms") == 0) {
        unsigned ms = (unsigned)arg_int(args, "ms", 1000, 100, 10000);
#ifdef _WIN32
        Sleep(ms);
#else
        {
            struct timespec ts;
            ts.tv_sec = ms / 1000u;
            ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
            nanosleep(&ts, NULL);
        }
#endif
        text = (char *)malloc(64);
        snprintf(text, 64, "waited %u ms", ms);
    } else {
        *is_error = 1;
        text = (char *)malloc(256);
        snprintf(text, 256, "unknown tool: %s", name);
    }

    result = *is_error ? obsmcp_error_result(text) : obsmcp_text_result(text);
    free(text);
    return result;
}
