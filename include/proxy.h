/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * proxy.h — Proxy inverse HTTP avec répartition de charge (round-robin)
 * et vérification périodique de l'état des backends.
 */

#ifndef LITH_PROXY_H
#define LITH_PROXY_H

#include <stddef.h>
#include "server.h"

void proxy_init(void);                       /* démarre les health checks si besoin */
int  proxy_match(const char *path);          /* index de route, ou -1 */
void proxy_handle(LithConn *c, HttpRequest *req, int route);

/* Écrit l'état des backends (JSON ou format Prometheus) dans buf. Retourne la longueur. */
size_t proxy_status_json(char *buf, size_t size);
size_t proxy_status_prometheus(char *buf, size_t size);

#endif /* LITH_PROXY_H */
