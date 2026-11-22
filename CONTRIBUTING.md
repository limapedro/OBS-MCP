# Contributing

- C11, warnings as errors (`/W4 /WX` with MSVC, `-Wall -Wextra -Werror` with
  gcc/clang).
- Build: `cmd /c scripts\build.bat` (Windows) or `sh scripts/build.sh`.
- Tests: `build\test_obs_mcp.exe` (or `build/test_obs_mcp`).
- Every bug fix needs a regression test.
- Never print anything but JSON-RPC to stdout. Use `fprintf(stderr, ...)`.
- Update docs/TOOLS.md when adding or changing a tool.
