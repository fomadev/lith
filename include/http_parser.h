/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>
#include <stdbool.h>

#define MAX_PATH_SIZE 256

typedef enum {
    METHOD_GET,
    METHOD_POST,
    METHOD_UNKNOWN
} HttpMethod;

typedef struct {
    HttpMethod method;
    char path[MAX_PATH_SIZE];
    long content_length;       // Longueur du corps annoncée par l'en-tête
    const char *body_start;    // Pointeur direct vers le début du corps dans le buffer initial
    bool keep_alive;           // AJOUT : État de persistance de la connexion
} HttpRequest;

/* Fonctions de parsing */
int parse_http_request(const char *buffer, HttpRequest *req);
const char* method_to_str(HttpMethod method);

/* Gestion dynamique des types MIME */
const char* get_mime_type(const char *path);

/* Génération centralisée de pages d'erreur HTML stylisées */
const char* get_error_html(int status_code, const char *title, const char *message);

#endif // HTTP_PARSER_H