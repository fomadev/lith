/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * proxy.c — Proxy inverse.
 *
 *   Navigateur --> LITH (/api/...) --> application (Node, Python, Go...) sur 127.0.0.1:3000
 *
 * - Plusieurs backends par route : répartition "round-robin" (chacun son tour).
 * - Un backend qui ne répond pas est marqué "hors service" et évité ;
 *   un thread le reteste toutes les HEALTH_CHECK_INTERVAL secondes.
 * - LITH ajoute X-Forwarded-For / X-Forwarded-Host / X-Forwarded-Proto / X-Real-IP.
 * - Chaque échange proxifié utilise une connexion neuve vers le backend
 *   ("Connection: close"), ce qui reste simple et robuste.
 */

#include "proxy.h"
#include "config.h"
#include "logger.h"
#include "metrics.h"
#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>

#define CONNECT_TIMEOUT_MS 3000
#define RESP_HEAD_MAX      16384
#define RELAY_CHUNK        (64 * 1024)

typedef struct {
    atomic_bool healthy;
    atomic_int  active;
    atomic_llong requests;
    atomic_llong failures;
} BackendState;

static BackendState g_state[MAX_PROXY_ROUTES][MAX_PROXY_BACKENDS];
static atomic_uint  g_rr[MAX_PROXY_ROUTES];

/* ---------------------------------------------------------------------- */

static void mark_health(int r, int b, bool healthy) {
    bool was = atomic_exchange(&g_state[r][b].healthy, healthy);
    if (was != healthy) {
        const BackendConf *bc = &g_config.proxies[r].backends[b];
        if (healthy) lith_log(LOG_INFO, "Proxy : backend %s:%s de nouveau disponible", bc->host, bc->port);
        else lith_log(LOG_WARN, "Proxy : backend %s:%s hors service", bc->host, bc->port);
    }
}

static void *health_thread(void *arg) {
    (void)arg;
    while (!server_stopping()) {
        for (int r = 0; r < g_config.proxy_count; r++) {
            for (int b = 0; b < g_config.proxies[r].backend_count; b++) {
                const BackendConf *bc = &g_config.proxies[r].backends[b];
                socket_t s;
                bool ok = lith_connect_tcp(bc->host, bc->port, 2000, &s) == 0;
                if (ok) lith_socket_close(s);
                mark_health(r, b, ok);
            }
        }
        for (int waited = 0; waited < g_config.health_interval * 1000 && !server_stopping(); waited += 200)
            lith_sleep_ms(200);
    }
    return NULL;
}

void proxy_init(void) {
    for (int r = 0; r < MAX_PROXY_ROUTES; r++) {
        atomic_init(&g_rr[r], 0);
        for (int b = 0; b < MAX_PROXY_BACKENDS; b++) {
            atomic_init(&g_state[r][b].healthy, true);
            atomic_init(&g_state[r][b].active, 0);
            atomic_init(&g_state[r][b].requests, 0);
            atomic_init(&g_state[r][b].failures, 0);
        }
    }
    for (int r = 0; r < g_config.proxy_count; r++) {
        const ProxyRouteConf *pr = &g_config.proxies[r];
        for (int b = 0; b < pr->backend_count; b++)
            lith_log(LOG_INFO, "Proxy : %s -> http://%s:%s", pr->prefix, pr->backends[b].host, pr->backends[b].port);
    }
    if (g_config.proxy_count > 0 && g_config.health_interval > 0) {
        pthread_t th;
        if (pthread_create(&th, NULL, health_thread, NULL) == 0) pthread_detach(th);
    }
}

int proxy_match(const char *path) {
    int best = -1;
    size_t best_len = 0;
    for (int i = 0; i < g_config.proxy_count; i++) {
        const char *prefix = g_config.proxies[i].prefix;
        size_t n = strlen(prefix);
        bool match = (n == 1 && prefix[0] == '/') ||
                     (strncmp(path, prefix, n) == 0 && (path[n] == '\0' || path[n] == '/'));
        if (match && (best < 0 || n > best_len)) {
            best = i;
            best_len = n;
        }
    }
    return best;
}

/* En-têtes "hop-by-hop" : propres à une connexion, jamais retransmis. */
static bool is_hop_header(const char *name) {
    static const char *hop[] = { "Connection", "Keep-Alive", "Proxy-Connection", "Proxy-Authenticate",
                                 "Proxy-Authorization", "TE", "Trailer", "Transfer-Encoding", "Upgrade" };
    for (size_t i = 0; i < sizeof(hop) / sizeof(hop[0]); i++)
        if (!strcasecmp(name, hop[i])) return true;
    return false;
}

char *proxy_build_request(LithConn *c, HttpRequest *req, const BackendConf *bc, size_t *len) {
    StrBuf sb;
    sb_init(&sb);
    sb_printf(&sb, "%s %s HTTP/1.1\r\n", req->method, req->target);

    const char *host = http_header_get(req, "Host");
    const char *xff = NULL;
    for (int i = 0; i < req->header_count; i++) {
        const char *name = req->headers[i].name;
        if (is_hop_header(name) || !strcasecmp(name, "Content-Length") || !strcasecmp(name, "Expect")) continue;
        if (!strcasecmp(name, "X-Forwarded-For")) { xff = req->headers[i].value; continue; }
        if (!strcasecmp(name, "X-Forwarded-Host") || !strcasecmp(name, "X-Forwarded-Proto") ||
            !strcasecmp(name, "X-Real-IP")) continue;
        sb_printf(&sb, "%s: %s\r\n", name, req->headers[i].value);
    }
    if (!host) sb_printf(&sb, "Host: %s:%s\r\n", bc->host, bc->port);
    if (xff) sb_printf(&sb, "X-Forwarded-For: %s, %s\r\n", xff, c->ip);
    else sb_printf(&sb, "X-Forwarded-For: %s\r\n", c->ip);
    sb_printf(&sb, "X-Real-IP: %s\r\n", c->ip);
    if (host) sb_printf(&sb, "X-Forwarded-Host: %s\r\n", host);
    /* Le backend reçoit toujours du HTTP en clair : on lui dit comment le client est arrivé. */
    sb_printf(&sb, "X-Forwarded-Proto: %s\r\n", conn_is_https(c) ? "https" : "http");
    if (req->body_len > 0 || req->content_length >= 0)
        sb_printf(&sb, "Content-Length: %zu\r\n", req->body_len);
    sb_puts(&sb, "Connection: close\r\n\r\n");
    return sb_take(&sb, len);
}

/* Trouve la fin des en-têtes de réponse. */
static long find_head_end(const char *buf, size_t len) {
    for (size_t i = 3; i < len; i++)
        if (buf[i - 3] == '\r' && buf[i - 2] == '\n' && buf[i - 1] == '\r' && buf[i] == '\n') return (long)(i + 1);
    return -1;
}

/*
 * Relaie la réponse du backend vers le client. On réécrit seulement le bloc
 * d'en-têtes (Connection: close, Server conservé) puis on copie le corps tel quel.
 * Retourne 0 si une réponse a été transmise, -1 si rien n'a été reçu (=> 502/504).
 */
static int relay_response(LithConn *c, socket_t bs, bool *timed_out) {
    char *buf = (char *)malloc(RELAY_CHUNK > RESP_HEAD_MAX ? RELAY_CHUNK : RESP_HEAD_MAX);
    if (!buf) return -1;
    size_t have = 0;
    long head_end = -1;
    *timed_out = false;

    while (head_end < 0 && have < RESP_HEAD_MAX) {
        long n = lith_recv(bs, buf + have, RESP_HEAD_MAX - have);
        if (n <= 0) {
            if (n < 0 && lith_last_error_is_timeout()) *timed_out = true;
            free(buf);
            return -1;
        }
        have += (size_t)n;
        head_end = find_head_end(buf, have);
    }
    if (head_end < 0 || strncmp(buf, "HTTP/1.", 7) != 0 || have < 12) { free(buf); return -1; }

    int status = atoi(buf + 9);
    if (status < 100 || status > 599) { free(buf); return -1; }

    /* Réécriture des en-têtes */
    StrBuf sb;
    sb_init(&sb);
    const char *line = buf;
    const char *end = buf + head_end - 2;   /* avant la ligne vide finale */
    const char *eol = strstr(line, "\r\n");
    sb_append(&sb, line, (size_t)(eol - line));
    sb_puts(&sb, "\r\n");
    line = eol + 2;
    while (line < end) {
        eol = strstr(line, "\r\n");
        if (!eol || eol > end) break;
        const char *colon = memchr(line, ':', (size_t)(eol - line));
        bool skip = false;
        if (colon) {
            char name[64];
            size_t nl = (size_t)(colon - line);
            if (nl < sizeof(name)) {
                memcpy(name, line, nl);
                name[nl] = '\0';
                skip = !strcasecmp(name, "Connection") || !strcasecmp(name, "Keep-Alive") ||
                       !strcasecmp(name, "Proxy-Connection");
            }
        }
        if (!skip) {
            sb_append(&sb, line, (size_t)(eol - line));
            sb_puts(&sb, "\r\n");
        }
        line = eol + 2;
    }
    sb_puts(&sb, "Connection: close\r\n\r\n");

    size_t head_len = 0;
    char *head = sb_take(&sb, &head_len);
    c->status = status;
    c->headers_sent = true;
    c->keep_alive = false;
    c->force_close = true;
    if (!head || lith_stream_send_all(&c->io, head, head_len) != 0) {
        free(head);
        free(buf);
        return 0;
    }
    free(head);

    /* Corps déjà reçu avec les en-têtes, puis le reste jusqu'à la fermeture du backend. */
    size_t body_have = have - (size_t)head_end;
    if (body_have > 0) {
        if (lith_stream_send_all(&c->io, buf + head_end, body_have) != 0) { free(buf); return 0; }
        c->bytes_sent += (long long)body_have;
    }
    for (;;) {
        long n = lith_recv(bs, buf, RELAY_CHUNK);
        if (n <= 0) break;
        if (lith_stream_send_all(&c->io, buf, (size_t)n) != 0) break;
        c->bytes_sent += n;
    }
    free(buf);
    return 0;
}

void proxy_handle(LithConn *c, HttpRequest *req, int route) {
    const ProxyRouteConf *pr = &g_config.proxies[route];
    if (http_header_get(req, "Upgrade")) {
        resp_error(c, 501, "WebSocket proxying is not supported.");
        return;
    }

    unsigned start = atomic_fetch_add(&g_rr[route], 1);
    int n = pr->backend_count;

    /* Deux passes : d'abord les backends sains, puis tous (au cas où le health check se trompe). */
    for (int pass = 0; pass < 2; pass++) {
        for (int k = 0; k < n; k++) {
            int b = (int)((start + (unsigned)k) % (unsigned)n);
            BackendState *bs_state = &g_state[route][b];
            if (pass == 0 && !atomic_load(&bs_state->healthy)) continue;
            if (pass == 1 && atomic_load(&bs_state->healthy)) continue;

            const BackendConf *bc = &pr->backends[b];
            socket_t bs;
            if (lith_connect_tcp(bc->host, bc->port, CONNECT_TIMEOUT_MS, &bs) != 0) {
                atomic_fetch_add(&bs_state->failures, 1);
                mark_health(route, b, false);
                continue;
            }
            mark_health(route, b, true);
            atomic_fetch_add(&bs_state->active, 1);
            atomic_fetch_add(&bs_state->requests, 1);
            lith_socket_set_recv_timeout(bs, g_config.proxy_timeout * 1000);
            lith_socket_set_send_timeout(bs, g_config.proxy_timeout * 1000);

            size_t rlen = 0;
            char *request = proxy_build_request(c, req, bc, &rlen);
            int ok = request && lith_send_all(bs, request, rlen) == 0;
            free(request);
            if (ok && req->body_len > 0) ok = lith_send_all(bs, req->body, req->body_len) == 0;

            bool timed_out = false;
            int rc = ok ? relay_response(c, bs, &timed_out) : -1;
            lith_socket_close(bs);
            atomic_fetch_sub(&bs_state->active, 1);

            if (rc == 0) return;
            atomic_fetch_add(&bs_state->failures, 1);
            metrics_proxy_error();
            if (timed_out) {
                lith_log(LOG_WARN, "Proxy : %s:%s n'a pas répondu en %d s", bc->host, bc->port, g_config.proxy_timeout);
                resp_error(c, 504, NULL);
            } else {
                lith_log(LOG_WARN, "Proxy : réponse invalide de %s:%s", bc->host, bc->port);
                resp_error(c, 502, NULL);
            }
            return;   /* la requête a peut-être été traitée : on ne la rejoue pas */
        }
    }
    metrics_proxy_error();
    lith_log(LOG_WARN, "Proxy : aucun backend joignable pour %s", pr->prefix);
    resp_error(c, 502, NULL);
}

size_t proxy_status_json(char *buf, size_t size) {
    size_t len = 0;
#define EMIT(...) do { int _n = snprintf(buf + len, size - len, __VA_ARGS__); \
        if (_n > 0 && len + (size_t)_n < size) len += (size_t)_n; } while (0)
    EMIT("[");
    for (int r = 0; r < g_config.proxy_count; r++) {
        const ProxyRouteConf *pr = &g_config.proxies[r];
        for (int b = 0; b < pr->backend_count; b++) {
            BackendState *s = &g_state[r][b];
            EMIT("%s{\"route\":\"%s\",\"backend\":\"%s:%s\",\"healthy\":%s,\"active\":%d,\"requests\":%lld,\"failures\":%lld}",
                (r || b) ? "," : "", pr->prefix, pr->backends[b].host, pr->backends[b].port,
                atomic_load(&s->healthy) ? "true" : "false", atomic_load(&s->active),
                (long long)atomic_load(&s->requests), (long long)atomic_load(&s->failures));
        }
    }
    EMIT("]");
    return len;
}

size_t proxy_status_prometheus(char *buf, size_t size) {
    size_t len = 0;
    if (g_config.proxy_count == 0) { if (size) buf[0] = '\0'; return 0; }
    EMIT("# HELP lith_backend_up Etat du backend (1 = disponible).\n# TYPE lith_backend_up gauge\n");
    for (int r = 0; r < g_config.proxy_count; r++)
        for (int b = 0; b < g_config.proxies[r].backend_count; b++)
            EMIT("lith_backend_up{route=\"%s\",backend=\"%s:%s\"} %d\n", g_config.proxies[r].prefix,
                g_config.proxies[r].backends[b].host, g_config.proxies[r].backends[b].port,
                atomic_load(&g_state[r][b].healthy) ? 1 : 0);
    EMIT("# HELP lith_backend_requests_total Requetes transmises au backend.\n# TYPE lith_backend_requests_total counter\n");
    for (int r = 0; r < g_config.proxy_count; r++)
        for (int b = 0; b < g_config.proxies[r].backend_count; b++)
            EMIT("lith_backend_requests_total{route=\"%s\",backend=\"%s:%s\"} %lld\n", g_config.proxies[r].prefix,
                g_config.proxies[r].backends[b].host, g_config.proxies[r].backends[b].port,
                (long long)atomic_load(&g_state[r][b].requests));
#undef EMIT
    return len;
}
