/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "common.h"
#include "server.h"
#include "logger.h"
#include <signal.h>

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    lith_log(LOG_INFO, "Shutting down LITH server...");
    
#ifdef _WIN32
    WSACleanup();
#endif

    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    printf("----------------------------------------\n");
    printf("   LITH : Light HTTP Server v%s\n", LITH_VERSION);
    printf("   Status: Running\n");
    printf("----------------------------------------\n");

    // Chargement de la configuration externe
    ServerConfig config;
    load_config("lith.conf", &config);

    // Surcharge optionnelle via argument CLI historique (Priorité Maximale)
    if (argc > 1) {
        int cli_port = atoi(argv[1]);
        if (cli_port > 0) {
            lith_log(LOG_INFO, "CLI Argument detected: Overriding port to %d", cli_port);
            config.port = cli_port;
        }
    }

    int server_fd = lith_init_server(&config);
    if (server_fd < 0) {
        lith_log(LOG_ERROR, "Failed to start server");
        return 1;
    }

    lith_start_server(server_fd, &config);
    return 0;
}