/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * websocket.c — WebSocket (RFC 6455) + rechargement automatique du navigateur.
 *
 * Fonctionnement du LIVE_RELOAD :
 *   - chaque page HTML servie reçoit un petit <script> qui ouvre un WebSocket
 *     vers /__lith/livereload ;
 *   - un thread surveille le dossier public (toutes les secondes) ;
 *   - dès qu'un fichier change, chaque navigateur connecté reçoit "reload".
 *
 * Chaque client WebSocket a son propre thread léger, pour ne jamais occuper
 * un thread du pool principal (limité à WS_MAX_CLIENTS connexions).
 */

#include "websocket.h"
#include "config.h"
#include "lith_crypto.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define WS_GUID            "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAX_PAYLOAD     (64 * 1024)
#define WATCH_INTERVAL_MS  1000
#define WATCH_MAX_DEPTH    16
#define WATCH_MAX_FILES    20000

static atomic_int  g_ws_clients;
static atomic_uint g_reload_generation;

bool ws_is_upgrade_request(const HttpRequest *req) {
    if (strcmp(req->method, "GET") != 0) return false;
    const char *upgrade = http_header_get(req, "Upgrade");
    const char *conn = http_header_get(req, "Connection");
    const char *key = http_header_get(req, "Sec-WebSocket-Key");
    const char *version = http_header_get(req, "Sec-WebSocket-Version");
    return upgrade && http_header_has_token(upgrade, "websocket") &&
           conn && http_header_has_token(conn, "upgrade") &&
           key && *key && version && strcmp(version, "13") == 0;
}

bool ws_compute_accept(const char *key, char *out, size_t out_size) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s%s", key, WS_GUID);
    if (n < 0 || (size_t)n >= sizeof(buf)) return false;
    unsigned char digest[20];
    lith_sha1(buf, (size_t)n, digest);
    return lith_base64_encode(digest, 20, out, out_size) > 0;
}

int ws_send_frame(LithStream *s, int opcode, const void *data, size_t len) {
    unsigned char hdr[10];
    size_t hl = 0;
    hdr[hl++] = (unsigned char)(0x80 | (opcode & 0x0F));
    if (len < 126) {
        hdr[hl++] = (unsigned char)len;
    } else if (len <= 0xFFFF) {
        hdr[hl++] = 126;
        hdr[hl++] = (unsigned char)(len >> 8);
        hdr[hl++] = (unsigned char)len;
    } else {
        hdr[hl++] = 127;
        for (int i = 7; i >= 0; i--) hdr[hl++] = (unsigned char)((uint64_t)len >> (8 * i));
    }
    if (lith_stream_send_all(s, hdr, hl) != 0) return -1;
    if (len && lith_stream_send_all(s, data, len) != 0) return -1;
    return 0;
}

/* Lit exactement n octets. Retourne 1 = ok, 0 = délai dépassé sans rien lire, -1 = fermé/erreur. */
static int recv_exact(LithStream *s, unsigned char *buf, size_t n, bool allow_idle_timeout) {
    size_t got = 0;
    int retries = 0;
    while (got < n) {
        long r = lith_stream_recv(s, buf + got, n - got);
        if (r > 0) { got += (size_t)r; continue; }
        if (r < 0 && lith_last_error_is_timeout()) {
            if (got == 0 && allow_idle_timeout) return 0;
            if (++retries > 10 || server_stopping()) return -1;
            continue;
        }
        return -1;
    }
    return 1;
}

static void *ws_livereload_thread(void *arg) {
    LithStream *s = (LithStream *)arg;
    unsigned seen = atomic_load(&g_reload_generation);
    lith_socket_set_recv_timeout(s->sock, 1000);
    lith_socket_set_send_timeout(s->sock, 5000);
    unsigned char *payload = (unsigned char *)malloc(WS_MAX_PAYLOAD);

    while (payload) {
        if (server_stopping()) {
            unsigned char code[2] = { 0x03, 0xE9 };   /* 1001 : le serveur s'arrête */
            ws_send_frame(s, 0x8, code, 2);
            break;
        }
        unsigned now = atomic_load(&g_reload_generation);
        if (now != seen) {
            seen = now;
            if (ws_send_frame(s, 0x1, "reload", 6) != 0) break;
        }

        unsigned char hdr[2];
        int r = recv_exact(s, hdr, 2, true);
        if (r == 0) continue;
        if (r < 0) break;

        int opcode = hdr[0] & 0x0F;
        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t len = hdr[1] & 0x7F;
        if (len == 126) {
            unsigned char ext[2];
            if (recv_exact(s, ext, 2, false) < 0) break;
            len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (len == 127) {
            unsigned char ext[8];
            if (recv_exact(s, ext, 8, false) < 0) break;
            len = 0;
            for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
        }
        if (!masked || len > WS_MAX_PAYLOAD) {
            unsigned char code[2] = { 0x03, 0xEA };   /* 1002 : erreur de protocole */
            ws_send_frame(s, 0x8, code, 2);
            break;
        }
        unsigned char mask[4];
        if (recv_exact(s, mask, 4, false) < 0) break;
        if (len && recv_exact(s, payload, (size_t)len, false) < 0) break;
        for (uint64_t i = 0; i < len; i++) payload[i] ^= mask[i & 3];

        if (opcode == 0x8) {                       /* fermeture demandée par le client */
            ws_send_frame(s, 0x8, payload, len >= 2 ? 2 : 0);
            break;
        }
        if (opcode == 0x9) {                       /* ping -> pong */
            if (ws_send_frame(s, 0xA, payload, (size_t)len) != 0) break;
        }
        /* Les autres trames (texte, binaire, pong) sont ignorées. */
    }

    free(payload);
    lith_stream_close(s, false);
    free(s);
    atomic_fetch_sub(&g_ws_clients, 1);
    lith_log(LOG_DEBUG, "Live reload : client déconnecté (%d actif(s))", atomic_load(&g_ws_clients));
    return NULL;
}

void livereload_handle(LithConn *c, HttpRequest *req) {
    if (!ws_is_upgrade_request(req)) {
        RespHeaders h;
        resp_init(&h);
        resp_add(&h, "Upgrade", "websocket");
        resp_error_ex(c, 400, "A WebSocket upgrade request is expected here.", &h);
        return;
    }
    if (atomic_fetch_add(&g_ws_clients, 1) >= WS_MAX_CLIENTS) {
        atomic_fetch_sub(&g_ws_clients, 1);
        resp_error(c, 503, "Too many live reload clients.");
        return;
    }
    char accept[64];
    if (!ws_compute_accept(http_header_get(req, "Sec-WebSocket-Key"), accept, sizeof(accept))) {
        atomic_fetch_sub(&g_ws_clients, 1);
        resp_error(c, 400, NULL);
        return;
    }

    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Upgrade", "websocket");
    resp_add(&h, "Connection", "Upgrade");
    resp_add(&h, "Sec-WebSocket-Accept", "%s", accept);
    if (resp_send_head(c, 101, &h, -1) != 0) {
        atomic_fetch_sub(&g_ws_clients, 1);
        c->force_close = true;
        return;
    }

    LithStream *arg = (LithStream *)malloc(sizeof(LithStream));
    pthread_t th;
    if (!arg) {
        atomic_fetch_sub(&g_ws_clients, 1);
        c->force_close = true;
        return;
    }
    *arg = c->io;   /* la session TLS éventuelle suit la socket */
    if (pthread_create(&th, NULL, ws_livereload_thread, arg) != 0) {
        free(arg);
        atomic_fetch_sub(&g_ws_clients, 1);
        c->force_close = true;
        return;
    }
    pthread_detach(th);
    c->detached = true;   /* la socket appartient désormais au thread WebSocket */
    lith_log(LOG_DEBUG, "Live reload : client connecté depuis %s", c->ip);
}

int livereload_client_count(void) {
    return atomic_load(&g_ws_clients);
}

const char *livereload_script(void) {
    return "<script>/* LITH live reload */(function(){var p=location.protocol==='https:'?'wss:':'ws:';"
           "function c(){var w=new WebSocket(p+'//'+location.host+'/__lith/livereload');"
           "w.onmessage=function(e){if(e.data==='reload')location.reload();};"
           "w.onclose=function(){setTimeout(c,1000);};}c();})();</script>\n";
}

/* ---------------------------------------------------------------------- */
/* Surveillance du dossier public                                          */
/* ---------------------------------------------------------------------- */

static uint64_t fingerprint_dir(const char *dir, int depth, int *files) {
    uint64_t acc = 1469598103934665603ULL;
    if (depth > WATCH_MAX_DEPTH) return acc;
    lith_dir_t *d = lith_dir_open(dir);
    if (!d) return acc;
    const char *name;
    while ((name = lith_dir_next(d)) != NULL && *files < WATCH_MAX_FILES) {
        if (name[0] == '.' || strcmp(name, "node_modules") == 0) continue;
        char full[LITH_PATH_MAX * 2];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        lith_stat_t st;
        if (lith_stat(full, &st) != 0) continue;
        (*files)++;
        uint64_t h = 1469598103934665603ULL;
        for (const char *p = name; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
        h ^= (uint64_t)st.mtime * 31u + (uint64_t)st.size;
        if (st.is_dir) h ^= fingerprint_dir(full, depth + 1, files);
        acc += h * 0x9E3779B97F4A7C15ULL;   /* somme : indépendante de l'ordre de parcours */
    }
    lith_dir_close(d);
    return acc;
}

static void *watcher_thread(void *arg) {
    (void)arg;
    int files = 0;
    uint64_t last = fingerprint_dir(g_config.public_dir, 0, &files);
    lith_log(LOG_INFO, "Live reload actif : surveillance de '%s' (%d élément(s))", g_config.public_dir, files);
    while (!server_stopping()) {
        for (int waited = 0; waited < WATCH_INTERVAL_MS && !server_stopping(); waited += 100)
            lith_sleep_ms(100);
        files = 0;
        uint64_t now = fingerprint_dir(g_config.public_dir, 0, &files);
        if (now != last) {
            last = now;
            atomic_fetch_add(&g_reload_generation, 1);
            lith_log(LOG_INFO, "Modification détectée : rechargement de %d navigateur(s)", atomic_load(&g_ws_clients));
        }
    }
    return NULL;
}

void livereload_start(void) {
    pthread_t th;
    if (pthread_create(&th, NULL, watcher_thread, NULL) == 0) pthread_detach(th);
    else lith_log(LOG_WARN, "Impossible de démarrer la surveillance du live reload");
}
