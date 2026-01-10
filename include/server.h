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
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DEFAULT_PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 4096

typedef struct {
    socket_t client_socket;
    struct sockaddr_in client_address;
} ClientContext;

// Déclarations des fonctions uniquement
int lith_init_server(int port);
void lith_start_server(int server_fd);
void *lith_client_handler(void *arg);
void lith_close_socket(socket_t s);

#endif