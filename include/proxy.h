/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * proxy.h — Proxy inverse HTTP avec répartition de charge (round-robin)
 * et vérification périodique de l'état des backends.
 */

#ifndef LITH_PROXY_H
#define LITH_PROXY_H

#include <stddef.h>
#include "config.h"
#include "server.h"

void proxy_init(void);                       /* démarre les health checks si besoin */
int  proxy_match(const char *path);          /* index de route, ou -1 */
void proxy_handle(LithConn *c, HttpRequest *req, int route);

/* Requête transmise au backend (en-têtes X-Forwarded-* compris). Exposée pour les tests. */
char *proxy_build_request(LithConn *c, HttpRequest *req, const BackendConf *bc, size_t *len);

/* Écrit l'état des backends (JSON ou format Prometheus) dans buf. Retourne la longueur. */
size_t proxy_status_json(char *buf, size_t size);
size_t proxy_status_prometheus(char *buf, size_t size);

#endif /* LITH_PROXY_H */
