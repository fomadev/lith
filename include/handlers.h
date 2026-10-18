/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * handlers.h — Aiguillage des requêtes et gestionnaires principaux.
 */

#ifndef LITH_HANDLERS_H
#define LITH_HANDLERS_H

#include <stdbool.h>
#include "server.h"
#include "http.h"

/* Point d'entrée : décide quoi faire de chaque requête. */
void router_handle(LithConn *c, HttpRequest *req);

/* Fichiers statiques (GET/HEAD) depuis PUBLIC_DIR. */
void static_serve(LithConn *c, HttpRequest *req);

/* Authentification HTTP Basic. */
int  auth_find_rule(const char *path);                       /* -1 si aucune règle */
bool auth_check_basic(const HttpRequest *req, const char *credentials);
void auth_send_challenge(LithConn *c);

/* Limitation de débit par adresse IP (algorithme du seau à jetons). */
void ratelimit_init(int rate_per_sec, int burst);
bool ratelimit_allow(const char *ip);

#endif /* LITH_HANDLERS_H */
