/* Minimal OBS WebSocket v5 client. MIT licensed. */
#ifndef OBS_MCP_WS_H
#define OBS_MCP_WS_H

#include <stddef.h>

/* Send one OBS v5 Request and return the raw RequestResponse JSON.
 * On success returns 0 and allocates *out_response (caller frees).
 * On failure returns non-zero and fills err. */
int obsmcp_ws_call(const char *host, unsigned port, const char *request_json,
                   char **out_response, char *err, size_t err_cap);

#endif
