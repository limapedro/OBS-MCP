/* OBS-MCP entry point: stdio MCP server. MIT licensed. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/config.h"
#include "obs_mcp/mcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

static void print_help(void)
{
    printf("obs-mcp - MCP server for controlling portable OBS instances\n");
    printf("Usage: obs-mcp [--config <file.toml>]\n\n");
    printf("Options:\n");
    printf("  --config <path>  Typed TOML configuration file\n");
    printf("  --help           Print this message\n");
}

static char *read_line(FILE *stream, size_t max)
{
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    int c;
    if (!buf) return NULL;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) {
            char *grown;
            if (cap >= max) break;
            cap *= 2;
            if (cap > max) cap = max;
            grown = (char *)realloc(buf, cap);
            if (!grown) { free(buf); return NULL; }
            buf = grown;
        }
        if (c == '\n') break;
        buf[len++] = (char)c;
    }
    if (len == 0 && c == EOF) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv)
{
    obsmcp_config_t config;
    const char *config_path = NULL;
    int i;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "obs-mcp: WSAStartup failed\n");
        return 1;
    }
#endif

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else {
            fprintf(stderr, "obs-mcp: unknown argument %s (only --config and --help are allowed)\n", argv[i]);
            return 1;
        }
    }

    obsmcp_config_init(&config);
    if (config_path && obsmcp_config_load(&config, config_path))
        return 1;

    for (;;) {
        char *line = read_line(stdin, config.max_request_bytes);
        if (!line) break;
        if (*line) obsmcp_handle_line(&config, line, strlen(line));
        free(line);
    }
    return 0;
}
