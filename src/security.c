/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * security.c — Authentification HTTP Basic et limitation de débit par IP.
 */

#include "handlers.h"
#include "config.h"
#include "lith_crypto.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

/* ==========================================================================
 * Authentification Basic
 * ========================================================================== */

/* Vrai si `path` est égal au préfixe ou se trouve "dessous" (/admin, /admin/x mais pas /administrateur). */
static bool path_under_prefix(const char *path, const char *prefix) {
    size_t n = strlen(prefix);
    if (n == 1 && prefix[0] == '/') return true;
    if (strncmp(path, prefix, n) != 0) return false;
    return path[n] == '\0' || path[n] == '/';
}

int auth_find_rule(const char *path) {
    int best = -1;
    size_t best_len = 0;
    for (int i = 0; i < g_config.auth_count; i++) {
        size_t n = strlen(g_config.auth[i].prefix);
        if (path_under_prefix(path, g_config.auth[i].prefix) && (best < 0 || n > best_len)) {
            best = i;
            best_len = n;
        }
    }
    return best;
}

bool auth_check_basic(const HttpRequest *req, const char *credentials) {
    const char *h = http_header_get(req, "Authorization");
    if (!h || strncasecmp(h, "Basic ", 6) != 0) return false;
    const char *b64 = h + 6;
    while (*b64 == ' ') b64++;
    unsigned char decoded[512];
    long n = lith_base64_decode(b64, decoded, sizeof(decoded));
    if (n <= 0) return false;
    return lith_secure_equals((const char *)decoded, credentials);
}

void auth_send_challenge(LithConn *c) {
    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "WWW-Authenticate", "Basic realm=\"LITH\", charset=\"UTF-8\"");
    resp_error_ex(c, 401, NULL, &h);
}

/* ==========================================================================
 * Limitation de débit : seau à jetons par adresse IP
 *
 * Chaque IP dispose d'un seau de RATE_BURST jetons, rempli de RATE_LIMIT
 * jetons par seconde. Une requête consomme un jeton ; seau vide => 429.
 * ========================================================================== */

#define RL_SLOTS      8192
#define RL_PROBE      32
#define RL_IDLE_MS    60000

typedef struct {
    char     ip[64];
    double   tokens;
    uint64_t last_ms;
    bool     used;
} RateSlot;

static RateSlot       *g_slots;
static pthread_mutex_t g_rl_mu = PTHREAD_MUTEX_INITIALIZER;
static double          g_rate;
static double          g_burst;

void ratelimit_init(int rate_per_sec, int burst) {
    g_rate = rate_per_sec;
    g_burst = burst > 0 ? burst : rate_per_sec * 2;
    if (rate_per_sec > 0 && !g_slots) g_slots = (RateSlot *)calloc(RL_SLOTS, sizeof(RateSlot));
}

static uint32_t hash_ip(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

bool ratelimit_allow(const char *ip) {
    if (g_rate <= 0 || !g_slots) return true;
    uint64_t now = lith_now_ms();
    uint32_t base = hash_ip(ip) % RL_SLOTS;

    pthread_mutex_lock(&g_rl_mu);
    RateSlot *slot = NULL, *oldest = NULL;
    for (int i = 0; i < RL_PROBE; i++) {
        RateSlot *s = &g_slots[(base + (uint32_t)i) % RL_SLOTS];
        if (s->used && strcmp(s->ip, ip) == 0) { slot = s; break; }
        if (!s->used || now - s->last_ms > RL_IDLE_MS) {
            if (!oldest || oldest->used) oldest = s;
        } else if (!oldest || (oldest->used && s->last_ms < oldest->last_ms)) {
            oldest = s;
        }
    }
    if (!slot) {
        slot = oldest;
        snprintf(slot->ip, sizeof(slot->ip), "%s", ip);
        slot->tokens = g_burst;
        slot->last_ms = now;
        slot->used = true;
    }
    double elapsed = (double)(now - slot->last_ms) / 1000.0;
    slot->tokens += elapsed * g_rate;
    if (slot->tokens > g_burst) slot->tokens = g_burst;
    slot->last_ms = now;
    bool allowed = slot->tokens >= 1.0;
    if (allowed) slot->tokens -= 1.0;
    pthread_mutex_unlock(&g_rl_mu);
    return allowed;
}
