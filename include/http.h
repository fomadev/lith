/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * http.h — Analyse des requêtes HTTP/1.x et utilitaires du protocole.
 */

#ifndef LITH_HTTP_H
#define LITH_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define HTTP_MAX_HEADERS       64
#define HTTP_MAX_HEADER_BYTES  16384   /* ligne de requête + en-têtes */
#define HTTP_MAX_PATH          2048

typedef struct {
    const char *name;
    const char *value;
} HttpHeader;

typedef struct {
    char        method[16];
    const char *target;            /* cible brute, ex: "/a%20b.html?x=1" */
    char        raw_path[HTTP_MAX_PATH]; /* partie chemin de la cible, encodée */
    char        path[HTTP_MAX_PATH];     /* chemin décodé, ex: "/a b.html" */
    const char *query;             /* après '?', ou "" */
    int         version_minor;     /* HTTP/1.0 -> 0, HTTP/1.1 -> 1 */

    HttpHeader  headers[HTTP_MAX_HEADERS];
    int         header_count;

    long long   content_length;    /* -1 si absent */
    bool        chunked;
    bool        keep_alive;
    bool        is_head;

    char       *body;              /* alloué par le serveur, NUL-terminé */
    size_t      body_len;
} HttpRequest;

/*
 * Analyse le bloc d'en-têtes situé dans buf[0 .. header_len) (sans le "\r\n\r\n" final).
 * Le buffer est modifié (insertion de '\0'). Retourne 0, ou un code d'erreur HTTP
 * (400, 414, 431, 501, 505).
 */
int  http_parse_request(char *buf, size_t header_len, HttpRequest *req);

const char *http_header_get(const HttpRequest *req, const char *name);
bool http_header_has_token(const char *value, const char *token);
bool http_accepts_gzip(const HttpRequest *req);

/* Décodage %XX. Retourne la longueur, ou -1 (séquence invalide, %00, buffer trop petit). */
int  http_url_decode(const char *src, size_t len, char *dst, size_t dst_size);
/* Encodage d'un nom de fichier pour un lien href. */
void http_url_encode_path(const char *src, char *dst, size_t dst_size);
/* Échappement HTML (&, <, >, ", '). */
void http_html_escape(const char *src, char *dst, size_t dst_size);

/* Vrai si le chemin décodé ne tente pas de sortir du dossier public. */
bool http_path_is_safe(const char *path);
/* Vrai si un segment du chemin commence par '.' (sauf /.well-known/). */
bool http_path_has_hidden_segment(const char *path);

const char *http_status_text(int code);
const char *http_mime_type(const char *path);
bool        http_mime_is_compressible(const char *mime);
bool        http_method_is_known(const char *method);

void   http_format_date(time_t t, char *out, size_t size);
time_t http_parse_date(const char *s);   /* -1 si invalide */

/*
 * En-tête Range "bytes=a-b" (une seule plage).
 * Retourne 1 = plage valide (start/end remplis), 0 = ignorer (format non géré),
 * -1 = plage non satisfaisable (416).
 */
int http_parse_range(const char *value, int64_t size, int64_t *start, int64_t *end);

#endif /* LITH_HTTP_H */
