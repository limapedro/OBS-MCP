/* Minimal RFC6455 text-frame client for OBS WebSocket v5. MIT licensed. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/obs_ws.h"
#include "vendor/cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ws_socket;
#define WS_INVALID INVALID_SOCKET
#define ws_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int ws_socket;
#define WS_INVALID (-1)
#define ws_close close
#endif

#define WS_MAX_FRAME (64u * 1024u * 1024u)

static int read_exact(ws_socket s, unsigned char *buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        int n = recv(s, (char *)buf + got, (int)(len - got), 0);
        if (n <= 0) return 1;
        got += (size_t)n;
    }
    return 0;
}

static int send_all(ws_socket s, const unsigned char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(s, (const char *)buf + sent, (int)(len - sent), 0);
        if (n <= 0) return 1;
        sent += (size_t)n;
    }
    return 0;
}

/* Read one server text frame (server frames are unmasked). */
static int read_frame(ws_socket s, char **out, size_t *out_len)
{
    unsigned char head[2];
    unsigned char ext[8];
    unsigned long long len;
    char *payload;

    if (read_exact(s, head, 2)) return 1;
    if ((head[0] & 0x0F) != 1) return 2; /* only text frames expected */
    len = head[1] & 0x7Fu;
    if (len == 126) {
        if (read_exact(s, ext, 2)) return 1;
        len = ((unsigned long long)ext[0] << 8) | ext[1];
    } else if (len == 127) {
        if (read_exact(s, ext, 8)) return 1;
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];
    }
    if (len > WS_MAX_FRAME) return 3;
    payload = (char *)malloc((size_t)len + 1u);
    if (!payload) return 4;
    if (len && read_exact(s, (unsigned char *)payload, (size_t)len)) {
        free(payload);
        return 1;
    }
    payload[len] = '\0';
    *out = payload;
    *out_len = (size_t)len;
    return 0;
}

/* Send one masked client text frame. */
static int send_frame(ws_socket s, const char *text, size_t len)
{
    unsigned char header[14];
    unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    unsigned char *masked;
    size_t hlen = 2;
    size_t i;

    header[0] = 0x81;
    if (len < 126) {
        header[1] = (unsigned char)(0x80u | len);
    } else if (len < 65536) {
        header[1] = 0x80u | 126u;
        header[2] = (unsigned char)(len >> 8);
        header[3] = (unsigned char)(len & 0xff);
        hlen = 4;
    } else {
        header[1] = 0x80u | 127u;
        for (i = 0; i < 8; ++i)
            header[2 + i] = (unsigned char)(len >> (56 - 8 * i));
        hlen = 10;
    }
    memcpy(header + hlen, mask, 4);
    masked = (unsigned char *)malloc(len ? len : 1u);
    if (!masked) return 1;
    for (i = 0; i < len; ++i) masked[i] = (unsigned char)text[i] ^ mask[i % 4];

    int ok = send_all(s, header, hlen + 4) == 0 && (len == 0 || send_all(s, masked, len) == 0);
    free(masked);
    return ok ? 0 : 1;
}

int obsmcp_ws_call(const char *host, unsigned port, const char *request_json,
                   char **out_response, char *err, size_t err_cap)
{
    ws_socket s = WS_INVALID;
    struct sockaddr_in addr;
    char handshake[512];
    char response[8192];
    char *frame = NULL;
    size_t frame_len = 0;
    cJSON *identify = NULL;
    cJSON *request = NULL;
    char *identify_text = NULL;
    int rc = 1;

    if (err_cap) err[0] = '\0';
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == WS_INVALID) {
        snprintf(err, err_cap, "socket failed");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1 ||
        connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        snprintf(err, err_cap, "cannot connect to %s:%u", host, port);
        ws_close(s);
        return 1;
    }

    snprintf(handshake, sizeof(handshake),
             "GET / HTTP/1.1\r\nHost: %s:%u\r\nUpgrade: websocket\r\n"
             "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
             "Sec-WebSocket-Version: 13\r\n\r\n", host, port);
    if (send_all(s, (const unsigned char *)handshake, strlen(handshake))) {
        snprintf(err, err_cap, "handshake send failed");
        goto done;
    }
    {
        size_t used = 0;
        while (used < sizeof(response) - 1) {
            int n = recv(s, response + used, 1, 0);
            if (n <= 0) break;
            ++used;
            response[used] = '\0';
            if (used >= 4 && strcmp(response + used - 4, "\r\n\r\n") == 0) break;
        }
        if (strstr(response, "101") == NULL) {
            snprintf(err, err_cap, "websocket upgrade rejected");
            goto done;
        }
    }

    if (read_frame(s, &frame, &frame_len) != 0) {
        snprintf(err, err_cap, "Hello frame missing");
        goto done;
    }
    free(frame); frame = NULL;

    identify = cJSON_CreateObject();
    cJSON_AddNumberToObject(identify, "op", 1);
    {
        cJSON *d = cJSON_AddObjectToObject(identify, "d");
        cJSON_AddNumberToObject(d, "rpcVersion", 1);
    }
    identify_text = cJSON_PrintUnformatted(identify);
    if (send_frame(s, identify_text, strlen(identify_text))) {
        snprintf(err, err_cap, "Identify send failed");
        goto done;
    }

    request = cJSON_Parse(request_json);
    if (!request) {
        snprintf(err, err_cap, "invalid request JSON");
        goto done;
    }
    {
        cJSON *op = cJSON_GetObjectItemCaseSensitive(request, "op");
        if (!cJSON_IsNumber(op) || op->valueint != 6) {
            cJSON_AddNumberToObject(request, "op", 6);
        }
    }
    {
        char *request_text = cJSON_PrintUnformatted(request);
        int send_rc = send_frame(s, request_text, strlen(request_text));
        free(request_text);
        if (send_rc) {
            snprintf(err, err_cap, "Request send failed");
            goto done;
        }
    }

    for (;;) {
        cJSON *parsed;
        cJSON *op;
        if (read_frame(s, &frame, &frame_len) != 0) {
            snprintf(err, err_cap, "response frame missing");
            goto done;
        }
        parsed = cJSON_Parse(frame);
        if (!parsed) {
            free(frame); frame = NULL;
            continue;
        }
        op = cJSON_GetObjectItemCaseSensitive(parsed, "d");
        (void)op;
        if (cJSON_GetObjectItemCaseSensitive(parsed, "op") &&
            cJSON_GetObjectItemCaseSensitive(parsed, "op")->valueint == 7) {
            *out_response = frame;
            frame = NULL;
            cJSON_Delete(parsed);
            rc = 0;
            goto done;
        }
        cJSON_Delete(parsed);
        free(frame); frame = NULL;
    }

done:
    free(frame);
    free(identify_text);
    if (identify) cJSON_Delete(identify);
    if (request) cJSON_Delete(request);
    ws_close(s);
    return rc;
}
