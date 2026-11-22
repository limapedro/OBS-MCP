/* MCP stdio JSON-RPC transport. MIT licensed. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/mcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void obsmcp_write_response(cJSON *id, cJSON *result)
{
    cJSON *root = cJSON_CreateObject();
    char *text;
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    if (id) cJSON_AddItemToObject(root, "id", id);
    else cJSON_AddNullToObject(root, "id");
    cJSON_AddItemToObject(root, "result", result);
    text = cJSON_PrintUnformatted(root);
    if (text) {
        fputs(text, stdout);
        fputc('\n', stdout);
        fflush(stdout);
        free(text);
    }
    cJSON_Delete(root);
}

void obsmcp_write_error(cJSON *id, int code, const char *message)
{
    cJSON *error = cJSON_CreateObject();
    cJSON *root = cJSON_CreateObject();
    char *text;
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    if (id) cJSON_AddItemToObject(root, "id", cJSON_Duplicate(id, 1));
    else cJSON_AddNullToObject(root, "id");
    cJSON_AddItemToObject(root, "error", error);
    text = cJSON_PrintUnformatted(root);
    if (text) {
        fputs(text, stdout);
        fputc('\n', stdout);
        fflush(stdout);
        free(text);
    }
    cJSON_Delete(root);
}

int obsmcp_handle_line(obsmcp_config_t *config, const char *line, size_t len)
{
    cJSON *request = cJSON_ParseWithLength(line, len);
    cJSON *id;
    cJSON *method;
    cJSON *params;

    if (!request) {
        obsmcp_write_error(NULL, -32700, "parse error");
        return 1;
    }
    id = cJSON_GetObjectItemCaseSensitive(request, "id");
    method = cJSON_GetObjectItemCaseSensitive(request, "method");
    params = cJSON_GetObjectItemCaseSensitive(request, "params");

    if (!cJSON_IsString(method)) {
        obsmcp_write_error(id, -32600, "method must be a string");
        cJSON_Delete(request);
        return 1;
    }

    if (strcmp(method->valuestring, "notifications/initialized") == 0 ||
        strcmp(method->valuestring, "initialized") == 0) {
        cJSON_Delete(request);
        return 0; /* notifications have no response */
    }

    if (strcmp(method->valuestring, "initialize") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON *caps = cJSON_CreateObject();
        cJSON *info = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
        cJSON_AddItemToObject(caps, "tools", cJSON_CreateObject());
        cJSON_AddItemToObject(result, "capabilities", caps);
        cJSON_AddStringToObject(info, "name", "obs-mcp");
        cJSON_AddStringToObject(info, "version", "0.1.0");
        cJSON_AddItemToObject(result, "serverInfo", info);
        obsmcp_write_response(id ? cJSON_Duplicate(id, 1) : NULL, result);
    } else if (strcmp(method->valuestring, "ping") == 0) {
        obsmcp_write_response(id ? cJSON_Duplicate(id, 1) : NULL, cJSON_CreateObject());
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON *tools = obsmcp_tools_list();
        cJSON_AddItemToObject(result, "tools", tools);
        obsmcp_write_response(id ? cJSON_Duplicate(id, 1) : NULL, result);
    } else if (strcmp(method->valuestring, "tools/call") == 0) {
        cJSON *name = params ? cJSON_GetObjectItemCaseSensitive(params, "name") : NULL;
        cJSON *arguments = params ? cJSON_GetObjectItemCaseSensitive(params, "arguments") : NULL;
        if (!cJSON_IsString(name)) {
            obsmcp_write_error(id, -32602, "tools/call needs a name");
        } else {
            int is_error = 0;
            cJSON *result = obsmcp_tool_call(config, name->valuestring, arguments, &is_error);
            (void)is_error;
            obsmcp_write_response(id ? cJSON_Duplicate(id, 1) : NULL, result);
        }
    } else {
        obsmcp_write_error(id, -32601, "method not found");
    }

    cJSON_Delete(request);
    return 0;
}
