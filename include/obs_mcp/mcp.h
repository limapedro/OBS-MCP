/* OBS-MCP JSON-RPC / MCP surface. MIT licensed. */
#ifndef OBS_MCP_MCP_H
#define OBS_MCP_MCP_H

#include "obs_mcp/config.h"
#include "vendor/cJSON/cJSON.h"

/* Process one newline-delimited JSON-RPC request. Returns 0 on success. */
int obsmcp_handle_line(obsmcp_config_t *config, const char *line, size_t len);

/* Write one JSON-RPC response line to stdout. */
void obsmcp_write_response(cJSON *id, cJSON *result);
void obsmcp_write_error(cJSON *id, int code, const char *message);

/* Tool helpers used by the dispatcher. */
cJSON *obsmcp_tools_list(void);
cJSON *obsmcp_tool_call(obsmcp_config_t *config, const char *name, cJSON *args, int *is_error);

/* Convenience: build an MCP text-content result. */
cJSON *obsmcp_text_result(const char *text);
cJSON *obsmcp_error_result(const char *text);

#endif
