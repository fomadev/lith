/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "server_utils.h"
#include "logger.h"
#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ClientContext base_ctx;
    char public_dir[256];
} ExpandedClientContext;

void *lith_client_handler(void *arg) {
    ExpandedClientContext *ectx = (ExpandedClientContext *)arg;
    ClientContext *ctx = &(ectx->base_ctx);
    char buffer[BUFFER_SIZE] = {0};
    
    int valread = recv(ctx->client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (valread > 0) {
        HttpRequest req;
        if (parse_http_request(buffer, &req) != 0) {
            lith_log(LOG_WARN, "400 - Bad Request parsing failed");
            const char *html = get_error_html(400, "Bad Request", "The server could not understand the request due to malformed syntax.");
            char header[256];
            sprintf(header, "HTTP/1.1 400 Bad Request\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", strlen(html));
            send(ctx->client_socket, header, (int)strlen(header), 0);
            send(ctx->client_socket, html, (int)strlen(html), 0);
            
            lith_close_socket(ctx->client_socket);
            free(ectx);
            return NULL;
        }

        lith_log(LOG_INFO, "Request: %s %s", method_to_str(req.method), req.path);

        if (!is_safe_path(req.path)) {
            lith_log(LOG_WARN, "Security Alert: Blocked traversal attempt on path: %s", req.path);
            const char *html = get_error_html(403, "Forbidden", "Access to this resource is strictly prohibited.");
            char header[256];
            sprintf(header, "HTTP/1.1 403 Forbidden\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", strlen(html));
            send(ctx->client_socket, header, (int)strlen(header), 0);
            send(ctx->client_socket, html, (int)strlen(html), 0);
            
            lith_close_socket(ctx->client_socket);
            free(ectx);
            return NULL;
        }

        char file_path[512];
        strncpy(file_path, ectx->public_dir, sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';

        if (strcmp(req.path, "/") == 0) {
            strcat(file_path, "/index.html");
        } else {
            strcat(file_path, req.path);
        }

        long file_size = 0;
        char *file_content = read_file(file_path, &file_size);

        if (file_content) {
            char header[384];
            const char *mime_type = get_mime_type(file_path);

            sprintf(header, 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %ld\r\n"
                    "Content-Type: %s\r\n"
                    "Connection: close\r\n\r\n", 
                    file_size, mime_type);

            send(ctx->client_socket, header, (int)strlen(header), 0);
            send(ctx->client_socket, file_content, (int)file_size, 0);
            free(file_content);
        } else {
            lith_log(LOG_WARN, "404 - Not Found: %s", file_path);
            const char *html = get_error_html(404, "Not Found", "The requested URL or resource could not be located on this server.");
            char header[256];
            sprintf(header, "HTTP/1.1 404 Not Found\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", strlen(html));
            send(ctx->client_socket, header, (int)strlen(header), 0);
            send(ctx->client_socket, html, (int)strlen(html), 0);
        }
    } else {
        lith_log(LOG_ERROR, "500 - Internal Server Error on network receive");
        const char *html = get_error_html(500, "Internal Server Error", "The server encountered an unexpected condition.");
        char header[256];
        sprintf(header, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", strlen(html));
        send(ctx->client_socket, header, (int)strlen(header), 0);
        send(ctx->client_socket, html, (int)strlen(html), 0);
    }

    lith_close_socket(ctx->client_socket);
    free(ectx);
    return NULL;
}

int lith_init_server(const ServerConfig *config) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        lith_log(LOG_ERROR, "WSAStartup failed");
        return -1;
    }
#endif

    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == (socket_t)-1) {
        lith_log(LOG_ERROR, "Socket creation failed");
        return -1;
    }

    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&opt, sizeof(opt)) < 0) {
        lith_log(LOG_WARN, "Failed to set SO_EXCLUSIVEADDRUSE");
    }
#else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
        lith_log(LOG_WARN, "Failed to set SO_REUSEADDR");
    }
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lith_log(LOG_ERROR, "Bind failed on port %d. Port might already be in use.", config->port);
        lith_close_socket(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        lith_log(LOG_ERROR, "Listen failed");
        lith_close_socket(server_fd);
        return -1;
    }

    lith_log(LOG_INFO, "LITH %s ready on port %d", LITH_VERSION, config->port);
    return (int)server_fd;
}

void lith_start_server(int server_fd, const ServerConfig *config) {
    while (true) {
        struct sockaddr_in addr;
#ifdef _WIN32
        int len = sizeof(addr);
#else
        socklen_t len = sizeof(addr);
#endif
        
        ExpandedClientContext *ectx = malloc(sizeof(ExpandedClientContext));
        if (!ectx) continue;

        strncpy(ectx->public_dir, config->public_dir, sizeof(ectx->public_dir) - 1);
        ectx->public_dir[sizeof(ectx->public_dir) - 1] = '\0';

        ectx->base_ctx.client_socket = accept((socket_t)server_fd, (struct sockaddr *)&addr, &len);

        if (ectx->base_ctx.client_socket == (socket_t)-1) {
            free(ectx);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, lith_client_handler, ectx) == 0) {
            pthread_detach(tid);
        } else {
            lith_close_socket(ectx->base_ctx.client_socket);
            free(ectx);
        }
    }
}