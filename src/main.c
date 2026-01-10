#include "common.h"
#include "server.h"
#include "logger.h"
#include <signal.h>

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    lith_log(LOG_INFO, "Shutting down LITH server...");
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    signal(SIGINT, handle_sigint);

    printf("----------------------------------------\n");
    printf("   LITH : Light HTTP Server v%s\n", LITH_VERSION);
    printf("   Status: Running (English mode)\n");
    printf("----------------------------------------\n");

    int server_fd = lith_init_server(port);
    if (server_fd < 0) {
        lith_log(LOG_ERROR, "Failed to start server");
        return 1;
    }

    lith_start_server(server_fd);
    return 0;
}