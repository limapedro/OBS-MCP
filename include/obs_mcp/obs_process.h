/* Portable OBS process control and sampling. MIT licensed. */
#ifndef OBS_MCP_PROCESS_H
#define OBS_MCP_PROCESS_H

#include "obs_mcp/config.h"

int obsmcp_launch(obsmcp_instance_t *inst, char *err, size_t err_cap);
int obsmcp_is_running(const obsmcp_instance_t *inst);
int obsmcp_terminate(obsmcp_instance_t *inst, char *err, size_t err_cap);
int obsmcp_cpu_stats(const obsmcp_instance_t *inst, unsigned window_ms,
                     double *cpu_seconds, double *cpu_percent, double *ws_mb,
                     char *err, size_t err_cap);
int obsmcp_log_tail(const obsmcp_instance_t *inst, unsigned lines,
                    char **out_text, char *err, size_t err_cap);

#endif
