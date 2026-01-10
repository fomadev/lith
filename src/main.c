#include "common.h"
#include "server.h"
#include "logger.h"
#include <signal.h>

/**
 * Gestionnaire de signal pour arrêter le serveur proprement (Ctrl+C)
 */
void handle_sigint(int sig) {
    (void)sig; // Évite l'avertissement "unused parameter"
    printf("\n");
    lith_log(LOG_INFO, "Arrêt de LITH en cours...");
    // Ici, nous pourrions fermer les sockets globalement si nécessaire
    exit(0);
}

int main(int argc, char *argv[]) {
    // 1. Définition du port (par défaut 8080 ou via argument)
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    // 2. Capture du signal d'interruption (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // 3. Titre de bienvenue dans la console
    printf("----------------------------------------\n");
    printf("   LITH : Light HTTP Server v%s\n", LITH_VERSION);
    printf("   Mode: Multi-threaded (pthreads)\n");
    printf("----------------------------------------\n");

    // 4. Initialisation du serveur
    int server_fd = lith_init_server(port);
    if (server_fd < 0) {
        lith_log(LOG_ERROR, "Impossible d'initialiser le serveur sur le port %d", port);
        return EXIT_FAILURE;
    }

    // 5. Lancement de la boucle d'écoute
    lith_start_server(server_fd);

    // Ne sera jamais atteint normalement
    close(server_fd);
    return EXIT_SUCCESS;
}