/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * router.c — Aiguillage de chaque requête, dans cet ordre :
 *
 *   1. limitation de débit           -> 429
 *   2. routes internes /__lith/...    (santé, statut, métriques, live reload)
 *   3. authentification (AUTH=...)    -> 401
 *   4. proxy inverse (PROXY=...)
 *   5. OPTIONS / méthodes non permises
 *   6. fichiers statiques
 */

#include "handlers.h"
#include "config.h"
#include "logger.h"
#include "metrics.h"
#include "proxy.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOWED_METHODS "GET, HEAD, OPTIONS"

/* Les pages de supervision : accès local, ou partout avec STATUS_AUTH. */
static bool status_access_ok(LithConn *c, HttpRequest *req) {
    if (g_config.status_auth[0]) {
        if (auth_check_basic(req, g_config.status_auth)) return true;
        auth_send_challenge(c);
        return false;
    }
    /* Derrière un proxy (Caddy, nginx...), tout arrive de 127.0.0.1 : une requête
     * relayée (X-Forwarded-For / Forwarded) n'est donc jamais considérée comme locale. */
    bool forwarded = http_header_get(req, "X-Forwarded-For") || http_header_get(req, "Forwarded");
    if (lith_ip_is_loopback(c->ip) && !forwarded) return true;
    resp_error(c, 403, "Status pages are only available from localhost (set STATUS_AUTH to allow remote access).");
    return false;
}

static void handle_internal(LithConn *c, HttpRequest *req) {
    const char *p = req->path + 7;   /* après "/__lith" */
    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Cache-Control", "no-store");

    if (strcmp(p, "/health") == 0) {
        resp_send(c, 200, "text/plain; charset=utf-8", "OK\n", 3, &h);
        return;
    }
    if (strcmp(p, "/livereload") == 0) {
        if (g_config.live_reload) livereload_handle(c, req);
        else resp_error(c, 404, "Live reload is disabled (LIVE_RELOAD=0).");
        return;
    }
    if (strcmp(p, "/status") == 0 || strcmp(p, "/metrics") == 0) {
        if (!g_config.status_enabled) { resp_error(c, 404, NULL); return; }
        if (!status_access_ok(c, req)) return;
        size_t len = 0;
        bool json = strcmp(p, "/status") == 0;
        char *doc = json ? metrics_status_json(&len) : metrics_prometheus(&len);
        if (!doc) { resp_error(c, 500, NULL); return; }
        resp_send(c, 200, json ? "application/json" : "text/plain; version=0.0.4; charset=utf-8", doc, len, &h);
        free(doc);
        return;
    }
    if (strcmp(p, "/echo") == 0) {
        /* Point de diagnostic : renvoie ce que LITH a compris de la requête. */
        if (strcmp(req->method, "POST") != 0) {
            RespHeaders a;
            resp_init(&a);
            resp_add(&a, "Allow", "POST");
            resp_error_ex(c, 405, NULL, &a);
            return;
        }
        const char *ct = http_header_get(req, "Content-Type");
        char body[512];
        int n = snprintf(body, sizeof(body),
                         "{\"status\":\"success\",\"method\":\"POST\",\"content_type\":\"%s\",\"bytes_received\":%zu}\n",
                         (ct && !strpbrk(ct, "\"\\")) ? ct : "", req->body_len);
        resp_send(c, 200, "application/json", body, (size_t)n, &h);
        return;
    }
    resp_error(c, 404, NULL);
}

/*
 * TLS_REDIRECT : sur le port HTTP, renvoie le client vers https://. Restent en
 * clair /__lith/health (sondes, `lith health`) et /.well-known/acme-challenge/
 * (validation Let's Encrypt). Retourne true si une réponse a été envoyée.
 */
static bool redirect_to_https(LithConn *c, HttpRequest *req) {
    if (!g_config.tls_redirect || conn_is_https(c)) return false;
    if (strcmp(req->path, "/__lith/health") == 0 || strncmp(req->path, "/.well-known/acme-challenge/", 28) == 0)
        return false;

    /* Nom d'hôte demandé, sans son port ; caractères limités pour éviter toute injection. */
    const char *host = http_header_get(req, "Host");
    char name[256];
    size_t n = 0;
    if (host && host[0] == '[') {
        const char *end = strchr(host, ']');
        n = end ? (size_t)(end - host) + 1 : 0;
    } else if (host) {
        n = strcspn(host, ":");
    }
    if (n == 0 || n >= sizeof(name) || strspn(host, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-[]:") < n) {
        resp_error(c, 400, "A valid Host header is required to redirect to HTTPS.");
        return true;
    }
    memcpy(name, host, n);
    name[n] = '\0';

    char port[16] = "";
    if (g_config.tls_port != 443) snprintf(port, sizeof(port), ":%d", g_config.tls_port);
    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Location", "https://%s%s%s", name, port, req->target[0] == '/' ? req->target : "/");
    /* 301 pour les lectures ; 308 conserve la méthode et le corps (POST...). */
    bool safe = !strcmp(req->method, "GET") || !strcmp(req->method, "HEAD");
    resp_send(c, safe ? 301 : 308, NULL, NULL, 0, &h);
    return true;
}

static void handle_options(LithConn *c, HttpRequest *req) {
    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Allow", ALLOWED_METHODS);
    if (g_config.cors_origin[0]) {
        resp_add(&h, "Access-Control-Allow-Methods", ALLOWED_METHODS);
        const char *rh = http_header_get(req, "Access-Control-Request-Headers");
        if (rh) resp_add(&h, "Access-Control-Allow-Headers", "%s", rh);
        resp_add(&h, "Access-Control-Max-Age", "600");
    }
    resp_send(c, 204, NULL, NULL, 0, &h);
}

void router_handle(LithConn *c, HttpRequest *req) {
    if (g_config.rate_limit > 0 && !ratelimit_allow(c->ip)) {
        metrics_rate_limited();
        lith_log(LOG_DEBUG, "Limite de débit atteinte pour %s", c->ip);
        RespHeaders h;
        resp_init(&h);
        resp_add(&h, "Retry-After", "1");
        resp_error_ex(c, 429, NULL, &h);
        return;
    }

    if (!http_method_is_known(req->method)) {
        resp_error(c, 501, NULL);
        return;
    }

    if (strcmp(req->path, "*") == 0) {   /* "OPTIONS * HTTP/1.1" */
        handle_options(c, req);
        return;
    }

    if (!http_path_is_safe(req->path)) {
        lith_log(LOG_WARN, "Chemin dangereux bloqué depuis %s : %s", c->ip, req->target);
        resp_error(c, 403, NULL);
        return;
    }

    if (redirect_to_https(c, req)) return;

    if (strncmp(req->path, "/__lith/", 8) == 0) {
        handle_internal(c, req);
        return;
    }

    int rule = auth_find_rule(req->path);
    if (rule >= 0 && !auth_check_basic(req, g_config.auth[rule].credentials)) {
        auth_send_challenge(c);
        return;
    }

    int route = proxy_match(req->path);
    if (route >= 0) {
        proxy_handle(c, req, route);
        return;
    }

    if (strcmp(req->method, "OPTIONS") == 0) {
        handle_options(c, req);
        return;
    }
    if (strcmp(req->method, "GET") != 0 && strcmp(req->method, "HEAD") != 0) {
        RespHeaders h;
        resp_init(&h);
        resp_add(&h, "Allow", ALLOWED_METHODS);
        resp_error_ex(c, 405, NULL, &h);
        return;
    }

    static_serve(c, req);
}
