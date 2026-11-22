# OBS-MCP architecture

## Contract

- Single C binary, no runtime dependencies beyond the C library and the
  platform socket/process APIs.
- MCP transport is **stdio**: exactly one JSON-RPC 2.0 message per line on
  stdout. Diagnostics go to stderr. This keeps the binary usable from any MCP
  client without a network port.
- Configuration is a typed TOML file loaded into a fixed struct with explicit
  defaults. The CLI accepts only `--config` and `--help`.
- No heap churn per request: cJSON parses each request into a tree that is
  deleted after the response is written. Parsing is bounded to 1 MiB.
- Network input is hostile: every socket read is bounded, framed, and checked.
  Malformed OBS WebSocket replies produce a JSON-RPC error, never a crash.

## Layers

```
stdio JSON-RPC loop  (src/mcp.c)
        |
        v
tool dispatcher      (src/tools.c)
        |
        +-- process control + CPU sampling   (src/obs_process.c)
        +-- Wormhole harness invocation      (src/harness.c)
        +-- OBS WebSocket v5 client          (src/obs_ws.c)
        |
        v
typed TOML config    (src/config.c)   cJSON (vendor/cJSON)
```

## Process model

The server does not link libobs. It controls OBS through:

1. the OS process API (`CreateProcessW` on Windows, `fork`/`execvp` on POSIX),
2. OBS's documented WebSocket v5 API (`obs-websocket`, built into OBS 28+),
3. log files under the portable profile's `config/obs-studio/logs/`.

This keeps the server independent of OBS's ABI and avoids the GPL linking
questions that come with libobs.

## Threading

Single-threaded by design. Each tool call is short; CPU sampling blocks for a
bounded window (default 1000 ms, max 10 s). Long operations are explicit tool
calls with a timeout, so an agent always gets a response.

## Error model

Every tool returns MCP's `content` array with either a JSON result or an
`isError: true` text payload. JSON-RPC errors are used only for protocol-level
failures (unknown method, bad params).
