/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#ifndef SERVER_H
#define SERVER_H

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
#endif

/* API de contrôle d'infrastructure - Mode Démon Linux (v1.0.8) */
#ifndef _WIN32
int lith_daemonize(void);
#endif

#include <pthread.h>
#include <stdbool.h>
#include "common.h" // Contient ServerConfig
#include "cache.h"  // Intégration du module de cache v1.0.9

#define DEFAULT_PORT 8090
#define BACKLOG 10
#define BUFFER_SIZE 4096

/* ==============================================================================
 * CONTEXTES CLIENTS
 * ============================================================================== */

typedef struct {
    socket_t client_socket;
    struct sockaddr_in client_address;
} ClientContext;

typedef struct {
    ClientContext base_ctx;
    char public_dir[256];
} ExpandedClientContext;

/* ==============================================================================
 * STRUCTURES DU THREAD POOL (v1.0.7)
 * ============================================================================== */

typedef struct Node {
    socket_t socket;
    struct Node *next;
} Node_t;

typedef struct {
    Node_t *head;           // Premier élément de la file (extraction)
    Node_t *tail;           // Dernier élément de la file (insertion)
    int size;               // Nombre actuel de connexions en attente
    pthread_mutex_t mutex;  // Mutex de protection de la file
    pthread_cond_t cond;    // Variable de condition pour réveiller les workers
    bool shutdown;          // Drapeau d'arrêt global du serveur
    char public_dir[256];   // Copie locale du répertoire public pour les workers
    LithCache ram_cache;    // <--- LE CACHE REJOINT LE CONTEXTE DES WORKERS (v1.0.9)
} ThreadPool_t;

/* ==============================================================================
 * PROTOTYPES ET CYCLE DE VIE
 * ============================================================================== */

/* Cycle de vie du serveur réseau */
int lith_init_server(const ServerConfig *config);
void lith_start_server(int server_fd, const ServerConfig *config);

/* Cœur du worker et traitement du protocole HTTP */
void *lith_client_handler(void *arg);

/* API de contrôle du Thread Pool */
void thread_pool_init(ThreadPool_t *pool, int num_threads, const char *public_dir);
void thread_pool_push(ThreadPool_t *pool, socket_t socket);
void thread_pool_shutdown(ThreadPool_t *pool);

#endif // SERVER_H