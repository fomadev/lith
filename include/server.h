#ifndef SERVER_H
#define SERVER_H

/* --- Gestion de la portabilité Windows / POSIX --- */
#ifdef _WIN32
    /* Architecture Windows */
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    
    /* On lie la bibliothèque Winsock (spécifique à MSVC/MinGW) */
    #pragma comment(lib, "ws2_32.lib")
    
    /* Compatibilité des types */
    typedef int socklen_t;
    #define close closesocket
    #define read(fd, buf, len) recv(fd, buf, len, 0)
    #define write(fd, buf, len) send(fd, buf, len, 0)
#else
    /* Architecture POSIX (Linux, macOS, WSL, Raspberry Pi) */
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include <pthread.h>

/* --- Configuration du Serveur --- */
#define DEFAULT_PORT 8080
#define BACKLOG 10          // File d'attente des connexions
#define BUFFER_SIZE 4096    // Augmenté pour supporter de plus grosses requêtes

/**
 * @brief Structure pour passer les données au thread de gestion client
 */
typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
} ClientContext;

/**
 * @brief Initialise le socket serveur, gère le bind et le listen
 * @param port Le port d'écoute
 * @return Le descripteur de socket, ou -1 en cas d'erreur
 */
int lith_init_server(int port);

/**
 * @brief Lance la boucle infinie d'acceptation des connexions (Multi-thread)
 * @param server_fd Le socket serveur actif
 */
void lith_start_server(int server_fd);

/**
 * @brief Fonction exécutée par chaque thread pour traiter le client
 */
void *lith_client_handler(void *arg);

#endif