/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "config.h"
#include "common.h"
#include "server_utils.h"
#include "logger.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Référence vers le pool global initialisé dans le main.c
extern ThreadPool_t global_pool;

/**
 * Initialise la couche socket basse couche du serveur (Windows/Linux)
 * Retourne le descripteur de fichier de la socket serveur ou -1 en cas d'erreur
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
        lith_close_socket(server_fd); // <-- Correction de la coquille ici
        return -1;
    }

    lith_log(LOG_INFO, "LITH %s ready on port %d", LITH_VERSION, config->port);
    return (int)server_fd;
}

/**
 * Boucle d'écoute principale (Accept-Loop)
 * Intercepte les sockets entrantes et les délègue immédiatement au Thread Pool
 */
void lith_start_server(int server_fd, const ServerConfig *config) {
    (void)config; // Évite le warning 'unused parameter' puisque le dossier public est géré par le pool
    
    lith_log(LOG_INFO, "LITH core engine event loop started. Waiting for connections...");

    while (true) {
        struct sockaddr_in addr;
#ifdef _WIN32
        int len = sizeof(addr);
#else
        socklen_t len = sizeof(addr);
#endif
        
        // 1. Blocage en attente d'une connexion cliente
        socket_t client_socket = accept((socket_t)server_fd, (struct sockaddr *)&addr, &len);

        if (client_socket == (socket_t)-1) {
            // Si le serveur est arrêté proprement, accept() peut retourner une erreur légitime
            if (global_pool.shutdown) {
                break;
            }
            continue;
        }

        // 2. Configuration du timeout SO_RCVTIMEO (Anti-Slowloris & Keep-Alive Window)
#ifdef _WIN32
        DWORD timeout = 3000; // 3 secondes en millisecondes
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 3;        // 3 secondes
        tv.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const struct timeval *)&tv, sizeof(tv));
#endif

        // 3. Injection instantanée dans la file d'attente concurrente du pool
        thread_pool_push(&global_pool, client_socket);
    }
}

/**
 * Initialise le pool de workers et pré-charge le cache statique RAM (v1.0.9)
 */
void thread_pool_init(ThreadPool_t *pool, int num_threads, const char *public_dir) {
    pool->head = NULL;
    pool->tail = NULL;
    pool->size = 0;
    pool->shutdown = false;
    
    // Sauvegarde du chemin du répertoire public
    strncpy(pool->public_dir, public_dir, sizeof(pool->public_dir) - 1);
    pool->public_dir[sizeof(pool->public_dir) - 1] = '\0';

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    // Initialisation et allocation du sous-système de Cache RAM
    lith_cache_init(&pool->ram_cache);
    
    // Warm-up : cartographie mémoire des fichiers stratégiques du dossier public/
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s/index.html", public_dir);
    lith_cache_add(&pool->ram_cache, path_buf, "text/html");
    
    snprintf(path_buf, sizeof(path_buf), "%s/style.css", public_dir);
    lith_cache_add(&pool->ram_cache, path_buf, "text/css");

    // Instanciation des Threads Workers fixes
    for (int i = 0; i < num_threads; i++) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, lith_client_handler, (void *)pool) == 0) {
            pthread_detach(thread); // Autogestion du cycle de vie des threads par le système
        }
    }
    
    lith_log(LOG_INFO, "Thread pool initialized successfully with %d active workers.", num_threads);
}