#include "server.h"
#include "common.h"
#include "logger.h"
#include "http_parser.h"

void lith_close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

/**
 * Checks if the requested path is safe (prevents Directory Traversal)
 * Returns true if safe, false if dangerous
 */
bool is_safe_path(const char *path) {
    if (!path) return false;

    // Reject paths containing ".." to prevent moving up directories
    if (strstr(path, "..") != NULL) {
        return false;
    }

    return true;
}

char* read_file(const char* filename, long *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(*size + 1);
    if (content) {
        fread(content, 1, *size, f);
        content[*size] = '\0';
    }
    fclose(f);
    return content;
}

void *lith_client_handler(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    char buffer[BUFFER_SIZE] = {0};
    
    int valread = recv(ctx->client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (valread > 0) {
        HttpRequest req;
        if (parse_http_request(buffer, &req) == 0) {
            lith_log(LOG_INFO, "Request: %s %s", method_to_str(req.method), req.path);

            // SECURITY CHECK: Anti-Directory Traversal
            if (!is_safe_path(req.path)) {
                lith_log(LOG_WARN, "Security Alert: Blocked traversal attempt on path: %s", req.path);
                char *res403 = "HTTP/1.1 403 Forbidden\r\nContent-Length: 22\r\nConnection: close\r\n\r\n<h1>403 Forbidden</h1>";
                send(ctx->client_socket, res403, (int)strlen(res403), 0);
                
                lith_close_socket(ctx->client_socket);
                free(ctx);
                return NULL;
            }

            char file_path[512] = "public";
            if (strcmp(req.path, "/") == 0) {
                strcat(file_path, "/index.html");
            } else {
                strcat(file_path, req.path);
            }

            long file_size = 0;
            char *file_content = read_file(file_path, &file_size);

            if (file_content) {
                char header[256];
                sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", file_size);
                send(ctx->client_socket, header, (int)strlen(header), 0);
                send(ctx->client_socket, file_content, (int)file_size, 0);
                free(file_content);
            } else {
                lith_log(LOG_WARN, "404 - Not Found: %s", file_path);
                char *res404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 20\r\nConnection: close\r\n\r\n<h1>404 Not Found</h1>";
                send(ctx->client_socket, res404, (int)strlen(res404), 0);
            }
        }
    }

    lith_close_socket(ctx->client_socket);
    free(ctx);
    return NULL;
}

int lith_init_server(int port) {
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
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lith_log(LOG_ERROR, "Bind failed on port %d", port);
        lith_close_socket(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        lith_log(LOG_ERROR, "Listen failed");
        lith_close_socket(server_fd);
        return -1;
    }

    lith_log(LOG_INFO, "LITH %s ready on port %d", LITH_VERSION, port);
    return (int)server_fd;
}

void lith_start_server(int server_fd) {
    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        
        ClientContext *ctx = malloc(sizeof(ClientContext));
        if (!ctx) continue;

        ctx->client_socket = accept((socket_t)server_fd, (struct sockaddr *)&addr, &len);

        if (ctx->client_socket == (socket_t)-1) {
            free(ctx);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, lith_client_handler, ctx) == 0) {
            pthread_detach(tid);
        } else {
            lith_close_socket(ctx->client_socket);
            free(ctx);
        }
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
}