#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "icp_agent/agent.h"

typedef struct {
    int  listener_fd;
    bool saw_path;
    bool saw_content_type;
    bool saw_self_describe_cbor;
} mock_server_ctx_t;

static const uint8_t QUERY_REPLIED_CBOR[] = {
    0xA2, 0x66, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x67, 0x72, 0x65, 0x70,
    0x6C, 0x69, 0x65, 0x64, 0x65, 0x72, 0x65, 0x70, 0x6C, 0x79, 0xA1, 0x63,
    0x61, 0x72, 0x67, 0x46, 0x44, 0x49, 0x44, 0x4C, 0x00, 0x00};

static void *mock_server_thread(void *arg) {
    mock_server_ctx_t *ctx = (mock_server_ctx_t *)arg;
    int                conn = -1;
    char               req[8192];
    ssize_t            n = 0;
    const char        *sep = NULL;
    char               resp_header[256];
    size_t             resp_header_len = 0;

    conn = accept(ctx->listener_fd, NULL, NULL);
    assert(conn >= 0);
    n = recv(conn, req, sizeof(req), 0);
    assert(n > 0);

    ctx->saw_path =
        strstr(req, "POST /api/v3/canister/aaaaa-aa/query HTTP/1.1") != NULL;
    ctx->saw_content_type =
        strstr(req, "Content-Type: application/cbor") != NULL;
    sep = strstr(req, "\r\n\r\n");
    if (sep != NULL) {
        const uint8_t *body = (const uint8_t *)(sep + 4);
        size_t         body_len = (size_t)(req + n - (const char *)body);
        ctx->saw_self_describe_cbor = body_len >= 3 && body[0] == 0xD9 &&
                                      body[1] == 0xD9 && body[2] == 0xF7;
    }

    resp_header_len = (size_t)snprintf(resp_header, sizeof(resp_header),
                                       "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: application/cbor\r\n"
                                       "Content-Length: %zu\r\n"
                                       "Connection: close\r\n"
                                       "\r\n",
                                       sizeof(QUERY_REPLIED_CBOR));
    send(conn, resp_header, resp_header_len, 0);
    send(conn, QUERY_REPLIED_CBOR, sizeof(QUERY_REPLIED_CBOR), 0);
    close(conn);
    return NULL;
}

int main(void) {
    int                  listener = -1;
    struct sockaddr_in   addr;
    socklen_t            addr_len = sizeof(addr);
    pthread_t            tid;
    mock_server_ctx_t    server = {0};
    char                 base_url[128];
    ic_agent_t           agent;
    uint8_t             *reply = NULL;
    size_t               reply_len = 0;
    static const uint8_t canister_id[] = {0, 0, 0, 0, 0, 0, 0x04, 0xD2};
    static const uint8_t arg[] = {'D', 'I', 'D', 'L', 0x00, 0x00};

    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    assert(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listener, 1) == 0);
    assert(getsockname(listener, (struct sockaddr *)&addr, &addr_len) == 0);

    server.listener_fd = listener;
    assert(pthread_create(&tid, NULL, mock_server_thread, &server) == 0);

    snprintf(base_url, sizeof(base_url), "http://127.0.0.1:%u",
             (unsigned)ntohs(addr.sin_port));

    assert(ic_agent_init(&agent, base_url) == IC_AGENT_OK);
    assert(ic_agent_query(&agent, canister_id, sizeof(canister_id), "hello",
                          arg, sizeof(arg), &reply, &reply_len) == IC_AGENT_OK);
    assert(reply != NULL);
    assert(reply_len == 6);
    assert(memcmp(reply, arg, reply_len) == 0);

    ic_agent_bytes_free(reply);
    ic_agent_destroy(&agent);

    assert(pthread_join(tid, NULL) == 0);
    close(listener);

    assert(server.saw_path);
    assert(server.saw_content_type);
    assert(server.saw_self_describe_cbor);
    return 0;
}
