# Gaming PC → Streaming PC workflow

This scenario simulates a two-machine OBS setup on one host using the two
portable instances and the Wormhole C harness proxy with a 40 Mbit/s cap.

## Configuration

- `config/gaming-streaming.toml` — instance paths, WebSocket ports
  (4455/4456), and the harness capture config.
- Wormhole `harness/config/obs-proxy-40m.toml` — proxy on 18080 → 18081,
  `bandwidth_bytes_per_sec = 5000000` (40 Mbit/s), captures both directions.

Sender profile: 2560x1440 base → 1920x1080 output, 60 fps (matches the
physical 2560x1440 display). Receiver profile: 1920x1080, 60 fps.

## Run

```bat
rem start the capped proxy once (keep it running)
echo {"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"capture_start","arguments":{}}} | build\obs-mcp.exe --config config\gaming-streaming.toml

rem launch both instances and stream for a fixed window
powershell -File scripts\gaming_streaming_workflow.ps1
```

The sender scene contains a Display Capture source (`monitor_capture` in OBS
33), a desktop-audio capture and two microphone captures. They can be created
through `obs_ws_call` with `CreateInput`; `docs/VERIFICATION.md` records the
resulting `GetInputList` and `GetSourceActive` responses.

## Recorded results

See Wormhole `exp_011/FINDINGS.md` for the full evidence. Summary:

- 1,427 packets (802 JPEG, 625 WAVE), 34,282,274 bytes in 13.35 s =
  **20.54 Mbit/s** average, below the 40 Mbit/s cap; byte-identical round-trip.
- All 802 frames decoded at 1920x1080.
- OBS stats: 60 fps, 0 skipped render frames on both instances; sender CPU
  4.27 % / 354.8 MB, receiver 2.84 % / 304.2 MB.
- Process CPU: sender 3.84 % / 371.8 MB, receiver 3.19 % / 312.0 MB.
