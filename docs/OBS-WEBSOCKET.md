# OBS WebSocket v5 usage

OBS Studio 28+ ships `obs-websocket` in the box. We enable it per portable
profile with:

```ini
[Websocket]
ServerEnabled=true
ServerPort=4455
ServerPassword=
```

The two test instances use ports 4455 (sender) and 4456 (receiver).

## Wire steps implemented

1. TCP connect to the configured port.
2. Read the `Hello` message (`op: 5`) and its `negotiateRpcVersion`,
   `sessionId` and security parameters.
3. Send `Identify` (`op: 1`) with `rpcVersion` (1) and empty auth if no
   password is configured.
4. Send `Request` (`op: 6`) with a JSON object containing `requestType`,
   `requestId` and optional `requestData`.
5. Read frames until `op: 7` (RequestResponse) with the matching `requestId`.

Frames are RFC 6455 text frames: client frames are masked, server frames are
not. The client supports payload lengths up to 64 MiB and rejects control
frames other than close/ping/pong (pong is answered).

## Reference

OBS WebSocket protocol documentation:
https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md
