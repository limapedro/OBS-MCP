# Verification evidence

Recorded 2026-11-22 on Windows 11 Pro, OBS Studio 33.0.0, MSVC 14.42.

## Build and unit tests

```
$ cmd /c scripts\build.bat
...
ok: config defaults
ok: config load
ok: tool list
ok: results
ALL OBS-MCP TESTS PASSED
build_exit=0
```

GitHub Actions CI (`.github/workflows/ci.yml`) is green on `windows-latest`,
`ubuntu-latest` and `macos-latest`, with MSVC `/W4 /WX` and
`cc -std=c11 -Wall -Wextra -Werror` respectively
(run 44992322175, conclusion `success`).

## MCP protocol smoke test

Requests: `initialize`, `notifications/initialized`, `tools/list`,
`tools/call obs_list_instances`, `tools/call bogus`.
Responses (one line each):

```
{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"obs-mcp","version":"0.1.0"}}}
{"jsonrpc":"2.0","id":2,"result":{"tools":[...10 tools...]}}
{"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"[{\"name\":\"sender\",\"running\":false,\"pid\":0,\"ws_port\":4455},{\"name\":\"receiver\",\"running\":false,\"pid\":0,\"ws_port\":4456}]"}]}}
{"jsonrpc":"2.0","id":4,"result":{"content":[{"type":"text","text":"unknown tool: bogus"}],"isError":true}}
```

The notification produced no response, as required.

## Live two-instance control

Requests: `obs_launch sender`, `obs_launch receiver`, `wait_ms 8000`,
`obs_list_instances`, `obs_ws_call GetVersion` on each,
`obs_cpu_stats sender`, `obs_log_tail sender`, `obs_terminate` on each.

Key results:

```
launched sender with pid 14384
launched receiver with pid 24992
[{"name":"sender","running":true,"pid":14384,"ws_port":4455},
 {"name":"receiver","running":true,"pid":24992,"ws_port":4456}]
GetVersion -> requestStatus.code=100, result=true,
              obsVersion=33.0.0, obsWebSocketVersion=5.7.6, rpcVersion=1
             (both instances)
cpu_seconds=0.094, cpu_percent=0.78, working_set_mb=257.7, window_ms=1000
log tail shows "[obs-websocket] New WebSocket client has connected"
terminated sender
terminated receiver
```

This demonstrates process control, real OBS API access, CPU/memory sampling,
log inspection and cleanup through a single MCP session.

## WebSocket client fix

An early C client did not consume OBS's `Identified` reply (op 2) after
`Identify`, which made larger replies such as `GetStats` fail with "response
frame missing". A Python `websocket-client` probe confirmed OBS answered
`GetStats` correctly (op 7, 435 bytes). After consuming the reply and handling
interleaved control frames, the C client returns the full `GetStats` response:
60.0 fps, 0 skipped render frames, CPU and memory fields populated.

## Throughput

Piping 1000 `ping` JSON-RPC requests through the server produced 1000 response
lines in 174.9 ms (~5,700 requests/s) on the dev machine; release binary size
is 234 KB. This is the "small and fast" claim: one C process, no runtime.

## Plugin dialog fix

Before the no-op module stub, OBS raised a `Plugin Errors` dialog on every
launch (verified with a Win32 window-title enumeration). After replacing the
AJA/DeckLink/multi-source-record binaries described in SETUP.md, the dialog no
longer appears, Teleport 0.8.0 loads (`[obs-teleport] version: 0.8.0`) and
scenes load normally.
