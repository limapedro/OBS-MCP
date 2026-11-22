# OBS-MCP

A small, fast **C implementation of a Model Context Protocol (MCP) server** for
controlling and observing two portable OBS Studio instances. It speaks MCP
(JSON-RPC 2.0 over stdio) and exposes tools that launch/stop instances, sample
CPU and memory, drive the Wormhole capture harness, and call OBS's built-in
WebSocket v5 API.

## Why

Automating OBS for network experiments previously required manual GUI steps and
ad-hoc PowerShell. OBS-MCP gives an agent a deterministic, scriptable control
surface with no Python/Node runtime, no garbage collector and a single C
binary. It is a sub-project of the Wormhole effort but is independent of it.

## Status

Phase 1: MCP stdio transport, typed TOML config, process control, CPU/memory
sampling, OBS WebSocket v5 client, harness invocation, and unit tests.

## Quick start

```bat
cmd /c scripts\build.bat
build\obs-mcp.exe --config config\example.toml
```

On Linux/macOS: `sh scripts/build.sh`.

The server writes **only JSON-RPC messages to stdout**; diagnostics go to
stderr. Send one JSON-RPC request per line on stdin, e.g.:

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1"}}}
{"jsonrpc":"2.0","id":2,"method":"tools/list"}
```

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — design and constraints
- [docs/TOOLS.md](docs/TOOLS.md) — every MCP tool, arguments and results
- [docs/OBS-WEBSOCKET.md](docs/OBS-WEBSOCKET.md) — OBS v5 API usage
- [docs/SETUP.md](docs/SETUP.md) — portable OBS setup, plugin cleanup, WebSocket config
- [docs/VERIFICATION.md](docs/VERIFICATION.md) — recorded build, MCP and live OBS evidence
- [docs/WORKFLOW.md](docs/WORKFLOW.md) — 1440p gaming PC → 1080p streaming PC scenario
- [CONTRIBUTING.md](CONTRIBUTING.md) — build/test conventions

## License

MIT. See [LICENSE](LICENSE). Vendored `cJSON` is MIT licensed; its notice is in
`vendor/cJSON/LICENSE`.
