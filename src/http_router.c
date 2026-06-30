/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "http_router.h"
#include "server_utils.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void send_http_error(socket_t client_socket, int status_code, const char *status_text, const char *description) {
    const char *html = get_error_html(status_code, status_text, description);
    char header[384];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", 
             status_code, status_text, strlen(html));
    send(client_socket, header, (int)strlen(header), 0);
    send(client_socket, html, (int)strlen(html), 0);
}

void handle_http_route(ExpandedClientContext *ectx, HttpRequest *req, char *full_buffer, int total_received) {
    ClientContext *ctx = &(ectx->base_ctx);
    (void)full_buffer; 
    (void)total_received;

    // CORRIGÉ : Accès par pointeur (->) pour le log
    lith_log(LOG_INFO, "Request: %s %s", method_to_str(req->method), req->path);

    // --- ROUTE API : POST ---
    // CORRIGÉ : Accès par pointeur (->) pour la méthode et le content_length
    if (req->method == METHOD_POST) {
        lith_log(LOG_INFO, "POST Payload size: %ld bytes", req->content_length);
        
        char response_body[512];
        snprintf(response_body, sizeof(response_body),
                 "{\"status\":\"success\",\"message\":\"Data received successfully by LITH\",\"bytes_processed\":%ld}",
                 req->content_length);

        char header[384];
        snprintf(header, sizeof(header), 
                 "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n", 
                 strlen(response_body));

        send(ctx->client_socket, header, (int)strlen(header), 0);
        send(ctx->client_socket, response_body, (int)strlen(response_body), 0);
    } 
    // --- ROUTE FICHIERS STATIQUES : GET ---
    // CORRIGÉ : Accès par pointeur (->) pour la méthode
    else if (req->method == METHOD_GET) {
        // CORRIGÉ : Accès par pointeur (->) pour le path
        if (!is_safe_path(req->path)) {
            lith_log(LOG_WARN, "Security Alert: Blocked traversal attempt on path: %s", req->path);
            send_http_error(ctx->client_socket, 403, "Forbidden", "Access to this resource is strictly prohibited.");
            return;
        }

        char file_path[512];
        strncpy(file_path, ectx->public_dir, sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';

        // CORRIGÉ : Accès par pointeur (->) pour la vérification de la racine
        if (strcmp(req->path, "/") == 0) {
            strcat(file_path, "/index.html");
        } else {
            strcat(file_path, req->path);
        }

        long file_size = 0;
        char *file_content = read_file(file_path, &file_size);

        if (file_content) {
            char header[384];
            const char *mime_type = get_mime_type(file_path);
            snprintf(header, sizeof(header), 
                     "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", 
                     file_size, mime_type);
            send(ctx->client_socket, header, (int)strlen(header), 0);
            send(ctx->client_socket, file_content, (int)file_size, 0);
            free(file_content);
        } else {
            lith_log(LOG_WARN, "404 - Not Found: %s", file_path);
            send_http_error(ctx->client_socket, 404, "Not Found", "The requested URL or resource could not be located on this server.");
        }
    }
}