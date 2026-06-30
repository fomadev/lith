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
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    typedef socklen_t system_socklen_t; // Évite les conflits de types selon les POSIX
#endif

#include <pthread.h>
#include <stdbool.h>
#include "common.h"

#define DEFAULT_PORT 8090
#define BACKLOG 10
#define BUFFER_SIZE 4096

typedef struct {
    socket_t client_socket;
    struct sockaddr_in client_address;
} ClientContext;

/* Cycle de vie du serveur réseau */
int lith_init_server(const ServerConfig *config);
void lith_start_server(int server_fd, const ServerConfig *config);
void *lith_client_handler(void *arg);

#endif // SERVER_H