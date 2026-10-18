/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * server.h — Cœur du serveur : connexions, pool de threads, réponses HTTP.
 */

#ifndef LITH_SERVER_H
#define LITH_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include "platform.h"
#include "http.h"
#include "tls.h"

/* Une connexion TCP cliente et l'état de la requête en cours. */
typedef struct {
    LithStream   io;             /* socket + session TLS éventuelle (HTTPS) */
    char         ip[64];
    int          requests_served;

    HttpRequest *req;            /* requête en cours (NULL si l'analyse a échoué) */
    int          status;         /* code HTTP envoyé */
    long long    bytes_sent;     /* octets de corps envoyés */
    bool         headers_sent;
    bool         keep_alive;     /* décidé au moment d'envoyer les en-têtes */
    bool         force_close;    /* fermer après cette réponse */
    bool         detached;       /* socket confiée à un autre thread (WebSocket) */
} LithConn;

static inline bool conn_is_https(const LithConn *c) { return lith_stream_is_tls(&c->io); }

/* En-têtes supplémentaires d'une réponse. */
typedef struct {
    char   buf[4096];
    size_t len;
} RespHeaders;

void resp_init(RespHeaders *h);
void resp_add(RespHeaders *h, const char *name, const char *fmt, ...) LITH_PRINTF(3, 4);

/* content_length < 0 : pas de Content-Length (la connexion sera fermée après le corps). */
int  resp_send_head(LithConn *c, int status, const RespHeaders *extra, long long content_length);
int  resp_send_body(LithConn *c, const void *data, size_t len);
int  resp_send(LithConn *c, int status, const char *content_type, const void *body, size_t len, const RespHeaders *extra);
void resp_error(LithConn *c, int status, const char *detail);
void resp_error_ex(LithConn *c, int status, const char *detail, const RespHeaders *extra);

/* Cycle de vie */
int  server_run(void);
void server_request_stop(void);
bool server_stopping(void);
bool server_is_busy(void);

#endif /* LITH_SERVER_H */
