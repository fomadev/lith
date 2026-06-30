/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "http_parser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * Convertit une valeur de l'énumération HttpMethod en sa représentation textuelle
 */
const char* method_to_str(HttpMethod method) {
    switch (method) {
        case METHOD_GET:  return "GET";
        case METHOD_POST: return "POST";
        default:          return "UNKNOWN";
    }
}

/**
 * Analyse une requête HTTP brute pour remplir la structure HttpRequest
 * Retourne 0 en cas de succès, -1 en cas d'échec
 */
int parse_http_request(const char *buffer, HttpRequest *req) {
    if (!buffer || !req) return -1;

    // 1. Initialisation par défaut et remise à zéro
    memset(req->path, 0, MAX_PATH_SIZE);
    req->content_length = 0;
    req->body_start = NULL;
    req->keep_alive = true; // Par défaut en HTTP/1.1 la connexion est persistante

    // 2. Détection de la méthode HTTP au début du buffer
    const char *ptr = buffer;
    if (strncmp(ptr, "GET ", 4) == 0) {
        req->method = METHOD_GET;
        ptr += 4;
    } else if (strncmp(ptr, "POST ", 5) == 0) {
        req->method = METHOD_POST;
        ptr += 5;
    } else {
        req->method = METHOD_UNKNOWN;
        return -1;
    }

    // 3. Extraction sécurisée du chemin (Path)
    int i = 0;
    while (*ptr && !isspace((unsigned char)*ptr) && i < (MAX_PATH_SIZE - 1)) {
        req->path[i++] = *ptr++;
    }
    req->path[i] = '\0';

    // 4. Extraction du Content-Length (insensible à la casse standard)
    const char *cl_ptr = strstr(buffer, "Content-Length:");
    if (!cl_ptr) {
        cl_ptr = strstr(buffer, "content-length:");
    }
    if (cl_ptr) {
        cl_ptr += 15; // On avance après la chaîne "Content-Length:"
        while (*cl_ptr == ' ') cl_ptr++; // Saute les espaces blancs optionnels
        req->content_length = strtol(cl_ptr, NULL, 10);
    }

    // 5. Extraction et validation de l'état Connection (Keep-Alive vs Close)
    const char *conn_ptr = strstr(buffer, "Connection:");
    if (!conn_ptr) {
        conn_ptr = strstr(buffer, "connection:");
    }
    if (conn_ptr) {
        conn_ptr += 11; // On avance après "Connection:"
        while (*conn_ptr == ' ') conn_ptr++; // Saute les espaces blancs
        
        if (strncmp(conn_ptr, "close", 5) == 0 || strncmp(conn_ptr, "Close", 5) == 0) {
            req->keep_alive = false;
        } else if (strncmp(conn_ptr, "keep-alive", 10) == 0 || strncmp(conn_ptr, "Keep-Alive", 10) == 0) {
            req->keep_alive = true;
        }
    }

    // 6. Localisation du début du Body (immédiatement après la double fin de ligne)
    const char *body_marker = strstr(buffer, "\r\n\r\n");
    if (body_marker) {
        req->body_start = body_marker + 4; // On saute les 4 octets de "\r\n\r\n"
    }

    return 0;
}

/**
 * Extrait l'extension du fichier et retourne le type MIME correspondant
 */
const char* get_mime_type(const char *path) {
    if (!path) return "application/octet-stream";

    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0)  return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".txt") == 0) return "text/plain";

    return "application/octet-stream";
}

/**
 * Génère une page d'erreur HTML stylisée centralisée
 */
const char* get_error_html(int status_code, const char *title, const char *message) {
    static char error_buffer[2048];
    
    snprintf(error_buffer, sizeof(error_buffer),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>%d - %s</title>\n"
        "    <style>\n"
        "        body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }\n"
        "        .error-card { text-align: center; background: #1e293b; padding: 3rem; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); max-width: 450px; width: 90%%; border: 1px solid rgba(239, 68, 68, 0.2); }\n"
        "        h1 { font-size: 4rem; margin: 0; color: #ef4444; font-weight: 800; }\n"
        "        h2 { font-size: 1.5rem; margin: 0.5rem 0 1rem 0; color: #e2e8f0; }\n"
        "        p { color: #94a3b8; line-height: 1.6; margin-bottom: 2rem; }\n"
        "        .footer { font-size: 0.8rem; color: #64748b; border-top: 1px solid #334155; padding-top: 1rem; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"error-card\">\n"
        "        <h1>%d</h1>\n"
        "        <h2>%s</h2>\n"
        "        <p>%s</p>\n"
        "        <div class=\"footer\">LITH Server • Powered by FomaDev</div>\n"
        "    </div>\n"
        "</body>\n"
        "</html>",
        status_code, title, status_code, title, message
    );
    
    return error_buffer;
}