/* OBS-MCP unit tests. MIT licensed. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/config.h"
#include "obs_mcp/mcp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_config_defaults(void)
{
    obsmcp_config_t config;
    obsmcp_config_init(&config);
    assert(strcmp(config.sender.name, "sender") == 0);
    assert(config.sender.ws_port == 4455);
    assert(config.receiver.ws_port == 4456);
    assert(config.max_request_bytes == OBSMCP_DEFAULT_MAX_REQUEST);
    printf("ok: config defaults\n");
}

static void test_config_load(void)
{
    const char *path = "build/test-config.toml";
    obsmcp_config_t config;
    FILE *f = fopen(path, "w");
    assert(f);
    fputs("[sender]\nname = \"alpha\"\nws_port = 9001\n\n[harness]\nexe = \"my-harness.exe\"\n", f);
    fclose(f);
    obsmcp_config_init(&config);
    assert(obsmcp_config_load(&config, path) == 0);
    assert(strcmp(config.sender.name, "alpha") == 0);
    assert(config.sender.ws_port == 9001);
    assert(strcmp(config.harness_exe, "my-harness.exe") == 0);
    assert(strcmp(config.receiver.name, "receiver") == 0);
    assert(obsmcp_instance(&config, "alpha") == &config.sender);
    assert(obsmcp_instance(&config, "nope") == NULL);
    printf("ok: config load\n");
}

static void test_tool_list(void)
{
    cJSON *tools = obsmcp_tools_list();
    assert(tools);
    assert(cJSON_GetArraySize(tools) == 10);
    cJSON_Delete(tools);
    printf("ok: tool list\n");
}

static void test_results(void)
{
    cJSON *r = obsmcp_text_result("hello");
    cJSON *content = cJSON_GetObjectItemCaseSensitive(r, "content");
    assert(content);
    assert(cJSON_GetArraySize(content) == 1);
    cJSON_Delete(r);
    r = obsmcp_error_result("bad");
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "isError")));
    cJSON_Delete(r);
    printf("ok: results\n");
}

int main(void)
{
    test_config_defaults();
    test_config_load();
    test_tool_list();
    test_results();
    printf("ALL OBS-MCP TESTS PASSED\n");
    return 0;
}
