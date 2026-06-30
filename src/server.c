/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "common.h"
#include "logger.h"
#include "http_parser.h"
#include <ctype.h>

/* Structure étendue passée au thread de traitement du client */
typedef struct {
    ClientContext base_ctx;
    char public_dir[256];
} ExpandedClientContext;

/**
 * Ferme un socket de manière portable (Windows / Unix)
 */
void lith_close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

/**
 * Vérifie si le chemin demandé est sécurisé (anti-Directory Traversal)
 * Retourne true si sûr, false si dangereux
 */
bool is_safe_path(const char *path) {
    if (!path) return false;

    // Rejette les chemins contenant ".." pour éviter de remonter l'arborescence
    if (strstr(path, "..") != NULL) {
        return false;
    }

    return true;
}

/**
 * Lit l'intégralité d'un fichier binaire et alloue la mémoire nécessaire
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
 * Charge un fichier de configuration clé=valeur basique
 * Supporte les commentaires avec '#' et ignore les espaces.
 */
int load_config(const char *filename, ServerConfig *config) {
    // Valeurs par défaut de secours
    config->port = 8090;
    strcpy(config->public_dir, "public");

    FILE *f = fopen(filename, "r");
    if (!f) {
        lith_log(LOG_WARN, "Configuration file '%s' not found. Using defaults (Port: %d, Root: %s)", 
                 filename, config->port, config->public_dir);
        return -1;
    }

    char line[384];
    while (fgets(line, sizeof(line), f)) {
        // Supprimer le changement de ligne terminal
        line[strcspn(line, "\r\n")] = 0;

        // Ignorer les commentaires et lignes vides
        char *ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0' || *ptr == '#') continue;

        // Séparer Clé / Valeur autour du signe '='
        char *equal = strchr(ptr, '=');
        if (!equal) continue;

        *equal = '\0';
        char *key = ptr;
        char *value = equal + 1;

        // Nettoyage des espaces pour la clé
        while (isspace((unsigned char)*key)) key++;
        char *end_key = key + strlen(key) - 1;
        while (end_key > key && isspace((unsigned char)*end_key)) { *end_key = '\0'; end_key--; }

        // Nettoyage des espaces pour la valeur
        while (isspace((unsigned char)*value)) value++;
        char *end_val = value + strlen(value) - 1;
        while (end_val > value && isspace((unsigned char)*end_val)) { *end_val = '\0'; end_val--; }

        // Affectation des paramètres
        if (strcmp(key, "PORT") == 0) {
            config->port = atoi(value);
        } else if (strcmp(key, "PUBLIC_DIR") == 0) {
            strncpy(config->public_dir, value, sizeof(config->public_dir) - 1);
            config->public_dir[sizeof(config->public_dir) - 1] = '\0';
        }
    }

    fclose(f);
    lith_log(LOG_INFO, "Configuration loaded: Port=%d, Public Directory='%s'", config->port, config->public_dir);
    return 0;
}

/**
 * Traitement des requêtes HTTP clients (Thread Worker)
 */
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

        // Utilisation dynamique du chemin public configuré au lieu de la chaine en dur "public"
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
        const char *html = get_error_html(500, "Internal Server Error", "The server encountered an unexpected condition during socket stream isolation.");
        char header[256];
        sprintf(header, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %zu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", strlen(html));
        send(ctx->client_socket, header, (int)strlen(header), 0);
        send(ctx->client_socket, html, (int)strlen(html), 0);
    }

    lith_close_socket(ctx->client_socket);
    free(ectx);
    return NULL;
}

/**
 * Initialisation du socket serveur d'écoute
 */
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
#endif
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
        lith_log(LOG_WARN, "Failed to set SO_REUSEADDR");
    }

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

/**
 * Boucle principale d'écoute des connexions entrantes
 */
void lith_start_server(int server_fd, const ServerConfig *config) {
    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        
        ExpandedClientContext *ectx = malloc(sizeof(ExpandedClientContext));
        if (!ectx) continue;

        // Recopie du dossier public dans le contexte du thread client
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