# MCP tools

All tools take a JSON object as `arguments` and return `{ "content": [ ... ] }`.

| Tool | Arguments | Result |
|---|---|---|
| `obs_list_instances` | none | configured instances with `running` flag and PID |
| `obs_launch` | `name` | PID of the launched instance |
| `obs_terminate` | `name` | success flag |
| `obs_cpu_stats` | `name`, `window_ms` (optional, 100–10000) | CPU delta seconds, percentage of machine, working set MB |
| `obs_ws_call` | `name`, `request` (JSON string with `op`/`d`) | WebSocket response JSON |
| `obs_log_tail` | `name`, `lines` (optional) | last N lines of the newest OBS log |
| `harness_run` | `command`, `config` | stdout of the Wormhole C harness command |
| `capture_start` | `config` | starts the C harness proxy as a child process |
| `capture_stop` | none | terminates the child proxy |
| `wait_ms` | `ms` (100–10000) | waits a bounded time; useful to let OBS become ready |

## Examples

```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"obs_ws_call","arguments":{"name":"sender","request":"{\"op\":6,\"d\":{\"requestType\":\"GetVersion\",\"requestId\":1}}"}}}
```

`op` values follow OBS WebSocket v5: 6 = Request, 7 = RequestResponse,
5 = Hello, 1 = Identify.
