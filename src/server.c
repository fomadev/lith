#include "server.h"
#include "common.h"
#include "logger.h"
#include "http_parser.h"
#include <pthread.h>

/**
 * Lit le contenu d'un fichier binaire ou texte.
 * @return Un pointeur vers le contenu (doit être libéré avec free())
 */
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

/**
 * Handler pour chaque client (Thread)
 */
void *lith_client_handler(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    char buffer[BUFFER_SIZE] = {0};
    
    int valread = recv(ctx->client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (valread > 0) {
        HttpRequest req;
        if (parse_http_request(buffer, &req) == 0) {
            lith_log(LOG_INFO, "Requête: %s %s", method_to_str(req.method), req.path);

            // Déterminer le chemin du fichier (ex: public/index.html)
            char file_path[512] = "public";
            if (strcmp(req.path, "/") == 0) {
                strcat(file_path, "/index.html");
            } else {
                strcat(file_path, req.path);
            }

            long file_size = 0;
            char *file_content = read_file(file_path, &file_size);

            if (file_content) {
                // En-tête HTTP 200 OK
                char header[256];
                sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", file_size);
                send(ctx->client_socket, header, (int)strlen(header), 0);
                
                // Corps du fichier
                send(ctx->client_socket, file_content, (int)file_size, 0);
                free(file_content);
            } else {
                // Fichier non trouvé : 404
                lith_log(LOG_WARN, "404 - Introuvable: %s", file_path);
                char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 18\r\n\r\n<h1>404 Page Not Found</h1>";
                send(ctx->client_socket, not_found, (int)strlen(not_found), 0);
            }
        }
    }

    close(ctx->client_socket);
    free(ctx);
    return NULL;
}

/**
 * Initialisation du socket serveur
 */
int lith_init_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        lith_log(LOG_ERROR, "Échec WSAStartup");
        return -1;
    }
#endif

    server_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        lith_log(LOG_ERROR, "Échec création socket");
        return -1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        lith_log(LOG_ERROR, "Bind impossible sur le port %d", port);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        lith_log(LOG_ERROR, "Listen échoué");
        return -1;
    }

    lith_log(LOG_INFO, "LITH %s prêt sur le port %d", LITH_VERSION, port);
    return server_fd;
}

/**
 * Boucle principale d'acceptation
 */
void lith_start_server(int server_fd) {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        ClientContext *ctx = malloc(sizeof(ClientContext));
        if (!ctx) continue;

        ctx->client_socket = (int)accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (ctx->client_socket < 0) {
            free(ctx);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, lith_client_handler, (void *)ctx) != 0) {
            close(ctx->client_socket);
            free(ctx);
        } else {
            pthread_detach(thread_id);
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif
}