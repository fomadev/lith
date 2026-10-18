/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * websocket.h — Protocole WebSocket (RFC 6455) et rechargement automatique
 * du navigateur (LIVE_RELOAD) qui s'appuie dessus.
 */

#ifndef LITH_WEBSOCKET_H
#define LITH_WEBSOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include "server.h"

#define WS_MAX_CLIENTS 64

bool ws_is_upgrade_request(const HttpRequest *req);
/* Calcule Sec-WebSocket-Accept à partir de Sec-WebSocket-Key (out >= 29 octets). */
bool ws_compute_accept(const char *key, char *out, size_t out_size);
/* Envoie une trame serveur -> client (non masquée). */
int  ws_send_frame(LithStream *s, int opcode, const void *data, size_t len);

/* Rechargement automatique */
void livereload_start(void);          /* lance la surveillance du dossier public */
void livereload_handle(LithConn *c, HttpRequest *req);
const char *livereload_script(void);  /* <script> injecté dans les pages HTML */
int  livereload_client_count(void);

#endif /* LITH_WEBSOCKET_H */
