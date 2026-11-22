#!/bin/sh
set -eu
CC=${CC:-cc}
mkdir -p build
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -I. -Iinclude \
    src/main.c src/mcp.c src/tools.c src/config.c src/obs_ws.c src/obs_process.c \
    vendor/cJSON/cJSON.c -o build/obs-mcp
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -I. -Iinclude \
    tests/test_obs_mcp.c src/mcp.c src/tools.c src/config.c src/obs_ws.c src/obs_process.c \
    vendor/cJSON/cJSON.c -o build/test_obs_mcp
build/test_obs_mcp
