/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "server_utils.h"
#include "logger.h"
#include "http_parser.h"
#include "http_router.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void *lith_client_handler(void *arg) {
    ExpandedClientContext *ectx = (ExpandedClientContext *)arg;
    ClientContext *ctx = &(ectx->base_ctx);
    
    char *full_buffer = malloc(BUFFER_SIZE * 4); 
    if (!full_buffer) {
        lith_log(LOG_ERROR, "Memory allocation failed for client handler");
        lith_close_socket(ctx->client_socket);
        free(ectx);
        return NULL;
    }

    bool connection_keep_active = true;
    int request_count = 0;
    const int MAX_KEEP_ALIVE_REQUESTS = 100; // Protection anti-monopolisation de thread

    while (connection_keep_active && request_count < MAX_KEEP_ALIVE_REQUESTS) {
        memset(full_buffer, 0, BUFFER_SIZE * 4);

        // Bloque au maximum pendant le timeout SO_RCVTIMEO (configuré à l'initialisation de la socket)
        int total_received = recv(ctx->client_socket, full_buffer, (BUFFER_SIZE * 4) - 1, 0);
        
        if (total_received > 0) {
            HttpRequest req;
            if (parse_http_request(full_buffer, &req) != 0) {
                lith_log(LOG_WARN, "400 - Bad Request: parsing failed");
                send_http_error(ctx->client_socket, 400, "Bad Request", "The server could not understand the request due to malformed syntax.");
                break; // On casse la persistance en cas d'erreur de protocole
            } 

            // --- GESTION DU CORPS DE LA REQUÊTE HTTP POST ---
            if (req.method == METHOD_POST && req.content_length > 0) {
                long already_received_body = 0;
                if (req.body_start) {
                    already_received_body = total_received - (req.body_start - full_buffer);
                }

                while (already_received_body < req.content_length) {
                    long remaining = req.content_length - already_received_body;
                    if (total_received >= (BUFFER_SIZE * 4) - 1) {
                        lith_log(LOG_WARN, "413 - Payload Too Large: Client bytes exceed allocated space");
                        break;
                    }

                    int n = recv(ctx->client_socket, full_buffer + total_received, remaining, 0);
                    if (n <= 0) {
                        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            lith_log(LOG_WARN, "Network receive timeout triggered during POST body streaming");
                        }
                        break;
                    }
                    total_received += n;
                    already_received_body += n;
                }
                parse_http_request(full_buffer, &req);
            }

            // Exécution du routage applicatif
            handle_http_route(ectx, &req, full_buffer, total_received);
            request_count++;

            // Mise à jour de la condition de boucle basée sur les désirs du client ou les erreurs
            connection_keep_active = req.keep_alive;

        } else {
            // total_received <= 0 : Déconnexion naturelle ou déclenchement du Timeout SO_RCVTIMEO
            if (total_received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (request_count == 0) {
                    // C'est un timeout sur la toute première requête (le client s'est connecté mais n'a rien envoyé)
                    lith_log(LOG_WARN, "408 - Request Timeout: Client failed to send data within the window");
                    send_http_error(ctx->client_socket, 408, "Request Timeout", "The server timed out waiting for the request.");
                } else {
                    // C'est un timeout Keep-Alive sain : la socket était ouverte en attente d'une n-ième requête
                    lith_log(LOG_INFO, "Keep-Alive window closed cleanly via network timeout");
                }
            }
            connection_keep_active = false;
        }
    }

    // Destruction unique des ressources en fin de vie de la connexion TCP persistante
    lith_close_socket(ctx->client_socket);
    free(full_buffer);
    free(ectx);
    return NULL;
}