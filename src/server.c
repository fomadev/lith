#include "server.h"
#include "common.h"
#include "logger.h"
#include "http_parser.h"
#include <pthread.h>

/**
 * Initialisation du socket serveur (Version Hybride)
 */
int lith_init_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // 1. Spécifique à Windows : Initialiser la DLL réseau
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        lith_log(LOG_ERROR, "Échec de WSAStartup (Code: %d)", WSAGetLastError());
        return -1;
    }
#endif

    // 2. Création du socket
    server_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        lith_log(LOG_ERROR, "Échec de création du socket");
        return -1;
    }

    // 3. Option de réutilisation du port
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 4. Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        lith_log(LOG_ERROR, "Bind impossible sur le port %d", port);
        return -1;
    }

    // 5. Listen
    if (listen(server_fd, BACKLOG) < 0) {
        lith_log(LOG_ERROR, "Listen échoué");
        return -1;
    }

    lith_log(LOG_INFO, "LITH démarré sur le port %d", port);
    return server_fd;
}

/**
 * Handler pour chaque client (Thread)
 */
void *lith_client_handler(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    char buffer[BUFFER_SIZE] = {0};
    
    // Utilise recv (plus portable que read pour les sockets Windows)
    int valread = recv(ctx->client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (valread > 0) {
        HttpRequest req;
        if (parse_http_request(buffer, &req) == 0) {
            lith_log(LOG_INFO, "Requête: %s %s", method_to_str(req.method), req.path);

            char *response = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html; charset=UTF-8\r\n"
                             "Connection: close\r\n\r\n"
                             "<h1>🚀 LITH Server</h1><p>Operationnel sur Windows/Linux!</p>";
            
            send(ctx->client_socket, response, (int)strlen(response), 0);
        }
    }

    // close est défini comme closesocket dans server.h pour Windows
    close(ctx->client_socket);
    free(ctx);
    return NULL;
}

/**
 * Boucle d'acceptation
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
    WSACleanup(); // Nettoyage final
#endif
}