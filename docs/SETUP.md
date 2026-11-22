# Setting up the two portable OBS instances

OBS-MCP controls two *disposable* portable OBS copies. Keep them under a
git-ignored directory outside OBS-MCP (the Wormhole project uses
`third_party/obs-portable/{sender,receiver}`).

## 1. Create the copies

Copy the OBS Studio install directory twice, add an empty `portable_mode.txt`
to the root of each copy, and create:
`config/obs-studio/basic/profiles/Wormhole/basic.ini` and
`config/obs-studio/basic/scenes/Wormhole{Sender,Receiver}.json`.

## 2. Replace modules that prevent a clean launch

OBS 33 bundles AJA/DeckLink hardware modules that fail to initialise on
machines without the hardware and raise a modal *Plugin Errors* dialog. They
cannot simply be deleted: OBS treats them as required core modules and aborts
startup with `Failed to load core module aja`.

The clean fix is to replace the failing DLLs with a tiny no-op module that
satisfies libobs' module ABI:

```
cl /LD /W4 /O2 vendor/obs_stub/obs_plugin_stub.c /Fe:obs_plugin_stub.dll
```

Then overwrite these files in **both** portable copies with that DLL:

- `core/aja/aja.dll`
- `core/aja-output-ui/aja-output-ui.dll`
- `core/decklink/decklink.dll`
- `core/decklink-captions/decklink-captions.dll`
- `core/decklink-output-ui/decklink-output-ui.dll`
- `obs-plugins/64bit/multi-source-record.dll`
- `obs-plugins/64bit/obs-shaderfilter.dll`

The stub exports `obs_module_set_pointer`, `obs_module_ver` and
`obs_module_load` (returning success). Result: OBS starts with no modal
dialog, and third-party plugins such as Teleport still load.

## 3. Enable OBS WebSocket v5

obs-websocket 5 stores settings in JSON, **not** in `basic.ini`:

`config/obs-studio/plugin_config/obs-websocket/config.json`

```json
{"first_load":false,"server_enabled":true,"server_port":4455,
 "alerts_enabled":false,"auth_required":false,"server_password":""}
```

Use port 4455 for the sender and 4456 for the receiver. Start OBS once and
confirm the log contains `WebSocket server started`.

## 4. Point OBS-MCP at the copies

Edit `config/wormhole.toml` (or your own config) so each instance's `obs_exe`,
`working_dir`, `log_dir` and `ws_port` match the portable copies.

## 5. Verify

Pipe the requests from `docs/VERIFICATION.md` into
`build/obs-mcp.exe --config config/wormhole.toml`. Every request must produce
exactly one JSON-RPC response line.
