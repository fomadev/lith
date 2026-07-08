/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

// Instanciation de l'entité globale du pool de threads définie dans server.h
ThreadPool_t global_pool;

/**
 * Gestionnaire du signal SIGINT (Ctrl+C)
 * Assure une extinction propre du serveur et du Thread Pool sans fuite de ressources
 */
void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    lith_log(LOG_INFO, "Initiating LITH orderly shutdown sequence...");
    
    // Extinction et libération propre du pool de threads
    thread_pool_shutdown(&global_pool);

#ifdef _WIN32
    WSACleanup();
#endif

    lith_log(LOG_INFO, "LITH server engine stopped successfully. Goodbye.");
    exit(0);
}

/**
 * Affiche l'aide utilisateur (EXPÉRIENCE DEVELOPPEUR - DX)
 */
void display_usage() {
    printf("======================================================================\n");
    printf(" LITH CLI - Minimalist High-Performance HTTP Engine v%s\n", LITH_VERSION);
    printf("======================================================================\n");
    printf("Usage: lith [command] [options]\n\n");
    printf("Commands:\n");
    printf("  start               Explicitly launch the HTTP server engine\n\n");
    printf("Options:\n");
    printf("  -v, --version       Print current stable release version\n");
    printf("  -h, --help          Display available flags and routing commands\n\n");
    printf("Example:\n");
    printf("  lith start 8080     Launch the engine overriding configuration port to 8080\n");
    printf("----------------------------------------------------------------------\n");
}

int main(int argc, char *argv[]) {
    // Interception de Ctrl+C pour le nettoyage des ressources concurrentielles
    signal(SIGINT, handle_sigint);

    // Cas 1 : Exécution sans argument (Aide contextuelle obligatoire - Issue #2)
    if (argc < 2) {
        display_usage();
        return 0;
    }

    // Cas 2 : Traitement des drapeaux globaux (Flags d'information)
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("LITH Engine Version: %s\n", LITH_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        display_usage();
        return 0;
    }

    // Cas 3 : Commande explicite de routage "start"
    if (strcmp(argv[1], "start") == 0) {
        // Chargement de la configuration système depuis le fichier de conf
        ServerConfig config;
        load_config("lith.conf", &config);

        // Surcharge dynamique du port via l'argument CLI (Priorité maximale)
        if (argc >= 3) {
            int port_override = atoi(argv[2]);
            if (port_override > 0) {
                lith_log(LOG_INFO, "CLI Command Argument detected: Overriding configuration port to %d", port_override);
                config.port = port_override;
            }
        }

        printf("----------------------------------------\n");
        printf("   LITH : Light HTTP Server v%s\n", LITH_VERSION);
        printf("   Status: Initializing Architecture...\n");
        printf("----------------------------------------\n");

        // Initialisation du Thread Pool (8 workers permanents isolés)
        // Les workers reçoivent dynamiquement le dossier public issu de la conf
        thread_pool_init(&global_pool, 8, config.public_dir);

        // Initialisation basse couche de la socket serveur (socket, bind, listen)
        int server_fd = lith_init_server(&config);
        if (server_fd < 0) {
            lith_log(LOG_ERROR, "Critical: Failed to initialize network socket layer");
            thread_pool_shutdown(&global_pool);
            return 1;
        }

        // Lancement de la boucle infinie d'acceptation (accept -> thread_pool_push)
        lith_start_server(server_fd, &config);
        
        return 0;
    }

    // Cas par défaut : Commande ou drapeau inconnu
    fprintf(stderr, "Error: Unknown command or option '%s'.\n\n", argv[1]);
    display_usage();
    return 1;
}