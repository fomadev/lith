/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "metrics.h"
#include "cache.h"
#include "config.h"
#include "platform.h"
#include "proxy.h"
#include "strbuf.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static struct {
    time_t        started_at;
    uint64_t      started_ms;
    int           workers;
    atomic_llong  connections_total;
    atomic_int    connections_active;
    atomic_llong  requests_total;
    atomic_llong  status_class[6];     /* index 1..5 => 1xx..5xx */
    atomic_llong  bytes_sent;
    atomic_llong  duration_ms_total;
    atomic_llong  cache_hits;
    atomic_llong  cache_misses;
    atomic_llong  rate_limited;
    atomic_llong  proxy_errors;
} M;

void metrics_init(int worker_threads) {
    M.started_at = time(NULL);
    M.started_ms = lith_now_ms();
    M.workers = worker_threads;
    atomic_init(&M.connections_total, 0);
    atomic_init(&M.connections_active, 0);
    atomic_init(&M.requests_total, 0);
    for (int i = 0; i < 6; i++) atomic_init(&M.status_class[i], 0);
    atomic_init(&M.bytes_sent, 0);
    atomic_init(&M.duration_ms_total, 0);
    atomic_init(&M.cache_hits, 0);
    atomic_init(&M.cache_misses, 0);
    atomic_init(&M.rate_limited, 0);
    atomic_init(&M.proxy_errors, 0);
}

void metrics_conn_open(void) {
    atomic_fetch_add(&M.connections_total, 1);
    atomic_fetch_add(&M.connections_active, 1);
}

void metrics_conn_close(void) {
    atomic_fetch_sub(&M.connections_active, 1);
}

void metrics_record_request(int status, long long bytes, uint64_t duration_ms) {
    atomic_fetch_add(&M.requests_total, 1);
    int cls = status / 100;
    if (cls >= 1 && cls <= 5) atomic_fetch_add(&M.status_class[cls], 1);
    if (bytes > 0) atomic_fetch_add(&M.bytes_sent, bytes);
    atomic_fetch_add(&M.duration_ms_total, (long long)duration_ms);
}

void metrics_cache(bool hit) {
    atomic_fetch_add(hit ? &M.cache_hits : &M.cache_misses, 1);
}

void metrics_rate_limited(void) { atomic_fetch_add(&M.rate_limited, 1); }
void metrics_proxy_error(void)  { atomic_fetch_add(&M.proxy_errors, 1); }

char *metrics_status_json(size_t *len) {
    uint64_t uptime = (lith_now_ms() - M.started_ms) / 1000;
    long long req = atomic_load(&M.requests_total);
    char started[40];
    struct tm tm;
    lith_gmtime(M.started_at, &tm);
    strftime(started, sizeof(started), "%Y-%m-%dT%H:%M:%SZ", &tm);

    StrBuf sb;
    sb_init(&sb);
    sb_printf(&sb, "{\n  \"server\": \"LITH\",\n  \"version\": \"%s\",\n", LITH_VERSION);
    sb_printf(&sb, "  \"started_at\": \"%s\",\n  \"uptime_seconds\": %llu,\n", started, (unsigned long long)uptime);
    sb_printf(&sb, "  \"pid\": %ld,\n  \"worker_threads\": %d,\n", lith_getpid(), M.workers);
    sb_printf(&sb, "  \"memory_rss_bytes\": %llu,\n", (unsigned long long)lith_rss_bytes());
    sb_printf(&sb, "  \"connections\": { \"active\": %d, \"total\": %lld },\n",
              atomic_load(&M.connections_active), (long long)atomic_load(&M.connections_total));
    sb_printf(&sb, "  \"requests\": { \"total\": %lld, \"1xx\": %lld, \"2xx\": %lld, \"3xx\": %lld, \"4xx\": %lld, \"5xx\": %lld, "
                   "\"rate_limited\": %lld, \"avg_duration_ms\": %.2f },\n",
              req, (long long)atomic_load(&M.status_class[1]), (long long)atomic_load(&M.status_class[2]),
              (long long)atomic_load(&M.status_class[3]), (long long)atomic_load(&M.status_class[4]),
              (long long)atomic_load(&M.status_class[5]), (long long)atomic_load(&M.rate_limited),
              req ? (double)atomic_load(&M.duration_ms_total) / (double)req : 0.0);
    sb_printf(&sb, "  \"bytes_sent\": %lld,\n", (long long)atomic_load(&M.bytes_sent));
    sb_printf(&sb, "  \"cache\": { \"enabled\": %s, \"entries\": %d, \"bytes\": %zu, \"hits\": %lld, \"misses\": %lld },\n",
              g_config.use_cache ? "true" : "false", cache_entry_count(), cache_total_bytes(),
              (long long)atomic_load(&M.cache_hits), (long long)atomic_load(&M.cache_misses));
    sb_printf(&sb, "  \"live_reload_clients\": %d,\n", livereload_client_count());
    char proxies[8192];
    proxy_status_json(proxies, sizeof(proxies));
    sb_printf(&sb, "  \"proxy\": { \"errors\": %lld, \"backends\": %s }\n}\n",
              (long long)atomic_load(&M.proxy_errors), proxies);
    return sb_take(&sb, len);
}

char *metrics_prometheus(size_t *len) {
    StrBuf sb;
    sb_init(&sb);
#define GAUGE(name, help, fmt, val)   sb_printf(&sb, "# HELP " name " " help "\n# TYPE " name " gauge\n" name " " fmt "\n", val)
#define COUNTER(name, help, fmt, val) sb_printf(&sb, "# HELP " name " " help "\n# TYPE " name " counter\n" name " " fmt "\n", val)
    sb_printf(&sb, "# HELP lith_info Informations sur la version.\n# TYPE lith_info gauge\nlith_info{version=\"%s\"} 1\n", LITH_VERSION);
    GAUGE("lith_uptime_seconds", "Secondes depuis le demarrage.", "%llu", (unsigned long long)((lith_now_ms() - M.started_ms) / 1000));
    GAUGE("lith_memory_rss_bytes", "Memoire residente du processus.", "%llu", (unsigned long long)lith_rss_bytes());
    GAUGE("lith_worker_threads", "Threads de travail.", "%d", M.workers);
    GAUGE("lith_connections_active", "Connexions ouvertes.", "%d", atomic_load(&M.connections_active));
    COUNTER("lith_connections_total", "Connexions acceptees.", "%lld", (long long)atomic_load(&M.connections_total));
    sb_puts(&sb, "# HELP lith_requests_total Requetes traitees par classe de statut.\n# TYPE lith_requests_total counter\n");
    for (int i = 1; i <= 5; i++)
        sb_printf(&sb, "lith_requests_total{class=\"%dxx\"} %lld\n", i, (long long)atomic_load(&M.status_class[i]));
    COUNTER("lith_request_duration_ms_total", "Somme des durees de traitement (ms).", "%lld", (long long)atomic_load(&M.duration_ms_total));
    COUNTER("lith_bytes_sent_total", "Octets de corps envoyes.", "%lld", (long long)atomic_load(&M.bytes_sent));
    COUNTER("lith_cache_hits_total", "Fichiers servis depuis la RAM.", "%lld", (long long)atomic_load(&M.cache_hits));
    COUNTER("lith_cache_misses_total", "Fichiers charges depuis le disque.", "%lld", (long long)atomic_load(&M.cache_misses));
    GAUGE("lith_cache_bytes", "Octets occupes par le cache.", "%zu", cache_total_bytes());
    COUNTER("lith_rate_limited_total", "Requetes refusees (429).", "%lld", (long long)atomic_load(&M.rate_limited));
    COUNTER("lith_proxy_errors_total", "Erreurs du proxy inverse.", "%lld", (long long)atomic_load(&M.proxy_errors));
#undef GAUGE
#undef COUNTER
    char proxies[8192];
    proxy_status_prometheus(proxies, sizeof(proxies));
    sb_puts(&sb, proxies);
    return sb_take(&sb, len);
}
