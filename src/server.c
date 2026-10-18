/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * server.c — Écoute réseau, pool de threads et cycle de vie d'une connexion.
 *
 *   thread principal : accept() --> file d'attente FIFO --> N threads de travail
 *
 * Chaque thread de travail prend une connexion, lit les requêtes (keep-alive
 * et pipelining HTTP/1.1 gérés), les confie au routeur, puis passe à la suivante.
 */

#include "server.h"
#include "cache.h"
#include "config.h"
#include "handlers.h"
#include "logger.h"
#include "metrics.h"
#include "proxy.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>

#define POLL_SLICE_MS     500
#define WORKER_STACK_SIZE (512 * 1024)

typedef struct Job {
    socket_t    sock;
    char        ip[64];
    struct Job *next;
} Job;

static struct {
    Job            *head;
    Job            *tail;
    pthread_mutex_t mu;
    pthread_cond_t  cond;
    bool            shutdown;
    pthread_t      *threads;
    int             nthreads;
} P;

static atomic_int g_stop;
static atomic_int g_queue_size;

void server_request_stop(void) { atomic_store(&g_stop, 1); }
bool server_stopping(void)     { return atomic_load(&g_stop) != 0; }
bool server_is_busy(void)      { return atomic_load(&g_queue_size) > 0; }

/* ---------------------------------------------------------------------- */
/* Journal d'accès                                                         */
/* ---------------------------------------------------------------------- */

static void sanitize(const char *in, char *out, size_t size) {
    size_t o = 0;
    for (const char *p = in; p && *p && o + 1 < size; p++)
        out[o++] = (*p == '"' || (unsigned char)*p < 0x20) ? '\'' : *p;
    out[o] = '\0';
}

static void log_access(const LithConn *c, const HttpRequest *req, uint64_t t0) {
    uint64_t dur = lith_now_ms() - t0;
    metrics_record_request(c->status, c->bytes_sent, dur);
    if (!g_config.access_log) return;

    char target[512], ua[256];
    sanitize(req && req->target ? req->target : "-", target, sizeof(target));
    sanitize(req ? http_header_get(req, "User-Agent") : NULL, ua, sizeof(ua));
    char line[1024];
    snprintf(line, sizeof(line), "%s \"%s %s HTTP/1.%d\" %d %lld %llums \"%s\"",
             c->ip, req ? req->method : "-", target, req ? req->version_minor : 1,
             c->status, c->bytes_sent, (unsigned long long)dur, ua[0] ? ua : "-");
    lith_log_access(line);
}

/* ---------------------------------------------------------------------- */
/* Traitement d'une connexion                                              */
/* ---------------------------------------------------------------------- */

static long find_header_end(const char *buf, size_t len, size_t from) {
    for (size_t i = from; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') return (long)i;
    return -1;
}

/* Fermeture "douce" après une erreur : laisse le client lire la réponse avant le reset TCP. */
static void lingering_close(socket_t s) {
#ifdef _WIN32
    shutdown(s, SD_SEND);
#else
    shutdown(s, SHUT_WR);
#endif
    char tmp[4096];
    for (int i = 0; i < 4; i++)
        if (lith_recv(s, tmp, sizeof(tmp)) <= 0) break;
    lith_socket_close(s);
}

static void handle_connection(socket_t sock, const char *ip) {
    metrics_conn_open();
    lith_socket_set_recv_timeout(sock, POLL_SLICE_MS);
    lith_socket_set_send_timeout(sock, 30000);
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));

    LithConn c;
    memset(&c, 0, sizeof(c));
    c.sock = sock;
    snprintf(c.ip, sizeof(c.ip), "%s", ip);

    char *buf = (char *)malloc(HTTP_MAX_HEADER_BYTES + 1);
    size_t have = 0;
    bool linger = false;

    while (buf) {
        /* ---- 1. Attendre un bloc d'en-têtes complet ---- */
        uint64_t wait_start = lith_now_ms();
        uint64_t first_byte = have ? wait_start : 0;
        long hdr_end = -1;
        size_t scan_from = 0;
        int idle_limit = (c.requests_served > 0 ? g_config.keepalive_timeout : g_config.request_timeout) * 1000;

        for (;;) {
            while (have >= 2 && buf[0] == '\r' && buf[1] == '\n') {   /* CRLF parasites entre requêtes */
                memmove(buf, buf + 2, have - 2);
                have -= 2;
                scan_from = 0;
            }
            hdr_end = find_header_end(buf, have, scan_from);
            if (hdr_end >= 0) break;
            if (have >= HTTP_MAX_HEADER_BYTES) {
                c.req = NULL;
                c.headers_sent = false;
                resp_error(&c, 431, NULL);
                log_access(&c, NULL, lith_now_ms());
                linger = true;
                goto done;
            }
            scan_from = have > 3 ? have - 3 : 0;

            long n = lith_recv(sock, buf + have, HTTP_MAX_HEADER_BYTES - have);
            if (n > 0) {
                if (have == 0) first_byte = lith_now_ms();
                have += (size_t)n;
                continue;
            }
            if (n < 0 && lith_last_error_is_timeout()) {
                uint64_t now = lith_now_ms();
                if (have == 0) {
                    if (server_stopping() || now - wait_start >= (uint64_t)idle_limit) goto done;
                } else if (now - first_byte >= (uint64_t)g_config.request_timeout * 1000) {
                    c.req = NULL;
                    c.headers_sent = false;
                    resp_error(&c, 408, NULL);
                    log_access(&c, NULL, now);
                    linger = true;
                    goto done;
                }
                continue;
            }
            goto done;   /* connexion fermée par le client ou erreur réseau */
        }

        /* ---- 2. Analyse ---- */
        HttpRequest req;
        req.body = NULL;
        req.body_len = 0;
        c.req = NULL;
        c.status = 0;
        c.bytes_sent = 0;
        c.headers_sent = false;
        c.keep_alive = false;
        c.force_close = false;
        uint64_t t0 = lith_now_ms();
        size_t consumed = (size_t)hdr_end + 4;

        int perr = http_parse_request(buf, (size_t)hdr_end, &req);
        if (perr) {
            lith_log(LOG_DEBUG, "Requête invalide depuis %s (%d)", c.ip, perr);
            resp_error(&c, perr, NULL);
            log_access(&c, NULL, t0);
            linger = true;
            goto done;
        }
        c.req = &req;

        /* ---- 3. Corps de la requête ---- */
        if (req.chunked) {
            resp_error(&c, 411, "Chunked request bodies are not supported, please send a Content-Length.");
            log_access(&c, &req, t0);
            linger = true;
            goto done;
        }
        if (req.content_length > g_config.max_body_size) {
            resp_error(&c, 413, NULL);
            log_access(&c, &req, t0);
            linger = true;
            goto done;
        }
        if (req.content_length > 0) {
            size_t cl = (size_t)req.content_length;
            req.body = (char *)malloc(cl + 1);
            if (!req.body) {
                resp_error(&c, 503, NULL);
                log_access(&c, &req, t0);
                goto done;
            }
            size_t avail = have - consumed;
            size_t take = avail < cl ? avail : cl;
            memcpy(req.body, buf + consumed, take);
            consumed += take;

            if (take < cl) {
                const char *expect = http_header_get(&req, "Expect");
                if (expect && strcasecmp(expect, "100-continue") == 0)
                    lith_send_all(sock, "HTTP/1.1 100 Continue\r\n\r\n", 25);
                size_t got = take;
                uint64_t last_activity = lith_now_ms();
                while (got < cl) {
                    long n = lith_recv(sock, req.body + got, cl - got);
                    if (n > 0) {
                        got += (size_t)n;
                        last_activity = lith_now_ms();
                        continue;
                    }
                    if (n < 0 && lith_last_error_is_timeout() && !server_stopping() &&
                        lith_now_ms() - last_activity < (uint64_t)g_config.request_timeout * 1000)
                        continue;
                    if (n < 0 && lith_last_error_is_timeout()) {
                        resp_error(&c, 408, NULL);
                        log_access(&c, &req, t0);
                    }
                    free(req.body);
                    goto done;
                }
            }
            req.body[cl] = '\0';
            req.body_len = cl;
        }

        /* ---- 4. Traitement ---- */
        router_handle(&c, &req);
        if (!c.headers_sent) resp_error(&c, 500, NULL);
        log_access(&c, &req, t0);
        free(req.body);
        req.body = NULL;
        c.requests_served++;

        if (c.detached) {             /* WebSocket : la socket vit désormais dans son propre thread */
            free(buf);
            metrics_conn_close();
            return;
        }
        if (!c.keep_alive) {
            linger = c.status >= 400;
            goto done;
        }

        /* ---- 5. Requêtes suivantes déjà reçues (pipelining) ---- */
        memmove(buf, buf + consumed, have - consumed);
        have -= consumed;
    }

done:
    free(buf);
    if (linger) lingering_close(sock);
    else lith_socket_close(sock);
    metrics_conn_close();
}

/* ---------------------------------------------------------------------- */
/* Pool de threads                                                         */
/* ---------------------------------------------------------------------- */

static void *worker_main(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&P.mu);
        while (!P.head && !P.shutdown) pthread_cond_wait(&P.cond, &P.mu);
        if (!P.head) {
            pthread_mutex_unlock(&P.mu);
            break;
        }
        Job *job = P.head;
        P.head = job->next;
        if (!P.head) P.tail = NULL;
        atomic_fetch_sub(&g_queue_size, 1);
        bool stopping = P.shutdown;
        pthread_mutex_unlock(&P.mu);

        if (stopping) lith_socket_close(job->sock);
        else handle_connection(job->sock, job->ip);
        free(job);
    }
    return NULL;
}

static bool pool_push(socket_t sock, const char *ip) {
    Job *job = (Job *)malloc(sizeof(Job));
    if (!job) return false;
    job->sock = sock;
    snprintf(job->ip, sizeof(job->ip), "%s", ip);
    job->next = NULL;
    pthread_mutex_lock(&P.mu);
    if (P.tail) P.tail->next = job;
    else P.head = job;
    P.tail = job;
    atomic_fetch_add(&g_queue_size, 1);
    pthread_cond_signal(&P.cond);
    pthread_mutex_unlock(&P.mu);
    return true;
}

static int pool_start(int n) {
    memset(&P, 0, sizeof(P));
    pthread_mutex_init(&P.mu, NULL);
    pthread_cond_init(&P.cond, NULL);
    P.threads = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    if (!P.threads) return -1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, WORKER_STACK_SIZE);
    for (int i = 0; i < n; i++) {
        if (pthread_create(&P.threads[i], &attr, worker_main, NULL) != 0) break;
        P.nthreads++;
    }
    pthread_attr_destroy(&attr);
    return P.nthreads > 0 ? 0 : -1;
}

static void pool_stop(void) {
    pthread_mutex_lock(&P.mu);
    P.shutdown = true;
    pthread_cond_broadcast(&P.cond);
    pthread_mutex_unlock(&P.mu);
    for (int i = 0; i < P.nthreads; i++) pthread_join(P.threads[i], NULL);
    free(P.threads);
    P.threads = NULL;
    pthread_mutex_destroy(&P.mu);
    pthread_cond_destroy(&P.cond);
}

/* ---------------------------------------------------------------------- */
/* Écoute                                                                  */
/* ---------------------------------------------------------------------- */

static socket_t open_listener(void) {
    char port[8];
    snprintf(port, sizeof(port), "%d", g_config.port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    const char *host = g_config.host;
    if (!strcmp(host, "*") || !host[0]) host = "0.0.0.0";

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0 || !res) {
        lith_log(LOG_ERROR, "Adresse d'écoute invalide : HOST=%s", g_config.host);
        return LITH_INVALID_SOCKET;
    }

    socket_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == LITH_INVALID_SOCKET) {
        freeaddrinfo(res);
        lith_log(LOG_ERROR, "Impossible de créer la socket d'écoute");
        return LITH_INVALID_SOCKET;
    }
    int opt = 1;
#ifdef _WIN32
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&opt, sizeof(opt));
#else
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#endif
    if (res->ai_family == AF_INET6) {
        int off = 0;   /* "::" écoute aussi en IPv4 */
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof(off));
    }
    if (bind(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        lith_log(LOG_ERROR, "Le port %d est déjà utilisé ou inaccessible (HOST=%s). Essayez un autre port : lith start 8091",
                 g_config.port, g_config.host);
        lith_socket_close(s);
        freeaddrinfo(res);
        return LITH_INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (listen(s, SOMAXCONN) != 0) {
        lith_log(LOG_ERROR, "listen() a échoué sur le port %d", g_config.port);
        lith_socket_close(s);
        return LITH_INVALID_SOCKET;
    }
    return s;
}

static void addr_to_ip(const struct sockaddr_storage *ss, char *out, size_t size) {
    if (ss->ss_family == AF_INET6) {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)ss;
        inet_ntop(AF_INET6, (void *)&a6->sin6_addr, out, (socklen_t)size);
        if (strncmp(out, "::ffff:", 7) == 0 && strchr(out + 7, '.'))
            memmove(out, out + 7, strlen(out + 7) + 1);   /* IPv4 vue en IPv6 -> forme IPv4 */
    } else {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)ss;
        inet_ntop(AF_INET, (void *)&a4->sin_addr, out, (socklen_t)size);
    }
}

static int auto_thread_count(void) {
    int n = lith_cpu_count() * 8;
    if (n < 16) n = 16;
    if (n > 128) n = 128;
    return n;
}

int server_run(void) {
    if (lith_net_init() != 0) {
        lith_log(LOG_ERROR, "Initialisation de la pile réseau impossible");
        return 1;
    }
    int threads = g_config.threads > 0 ? g_config.threads : auto_thread_count();

    metrics_init(threads);
    cache_init(g_config.use_cache ? (size_t)g_config.cache_max_size : 0, (size_t)g_config.cache_file_max);
    ratelimit_init(g_config.rate_limit, g_config.rate_burst);

    socket_t ls = open_listener();
    if (ls == LITH_INVALID_SOCKET) {
        lith_net_cleanup();
        return 1;
    }
    if (pool_start(threads) != 0) {
        lith_log(LOG_ERROR, "Impossible de créer les threads de travail");
        lith_socket_close(ls);
        lith_net_cleanup();
        return 1;
    }
    proxy_init();
    if (g_config.live_reload) livereload_start();

    const char *shown = (!strcmp(g_config.host, "0.0.0.0") || !strcmp(g_config.host, "::")) ? "localhost" : g_config.host;
    lith_log(LOG_INFO, "LITH %s prêt : http://%s:%d  (dossier '%s', %d threads, cache %s, gzip %s)",
             LITH_VERSION, shown, g_config.port, g_config.public_dir, P.nthreads,
             g_config.use_cache ? "on" : "off", g_config.gzip ? "on" : "off");
    lith_log(LOG_INFO, "Appuyez sur Ctrl+C pour arrêter.");

    while (!server_stopping()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ls, &rfds);
        struct timeval tv = { 0, POLL_SLICE_MS * 1000 };
        int r = select((int)ls + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) continue;

        struct sockaddr_storage ss;
        socklen_t sl = sizeof(ss);
        socket_t cs = accept(ls, (struct sockaddr *)&ss, &sl);
        if (cs == LITH_INVALID_SOCKET) continue;

        char ip[64] = "-";
        addr_to_ip(&ss, ip, sizeof(ip));

        if (atomic_load(&g_queue_size) >= g_config.max_pending) {
            static const char busy[] = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nRetry-After: 1\r\nConnection: close\r\n\r\n";
            lith_send_all(cs, busy, sizeof(busy) - 1);
            lith_socket_close(cs);
            lith_log(LOG_WARN, "File d'attente pleine (%d) : connexion de %s refusée", g_config.max_pending, ip);
            continue;
        }
        if (!pool_push(cs, ip)) lith_socket_close(cs);
    }

    lith_log(LOG_INFO, "Arrêt en cours : fin des requêtes en cours...");
    lith_socket_close(ls);
    pool_stop();
    cache_destroy();
    lith_net_cleanup();
    lith_log(LOG_INFO, "LITH est arrêté. À bientôt !");
    return 0;
}
