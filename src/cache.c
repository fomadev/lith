/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "cache.h"
#include "lith_crypto.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define CACHE_BUCKETS 1024

static CacheEntry     *g_table[CACHE_BUCKETS];
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static size_t          g_total;
static size_t          g_max_total;
static size_t          g_max_file;
static int             g_count;
static uint64_t        g_tick;

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

static void entry_free(CacheEntry *e) {
    free(e->path);
    free(e->data);
    free(e->gz);
    free(e);
}

static void detach_locked(CacheEntry *e) {
    uint32_t b = hash_str(e->path) % CACHE_BUCKETS;
    CacheEntry **pp = &g_table[b];
    while (*pp && *pp != e) pp = &(*pp)->hnext;
    if (*pp) *pp = e->hnext;
    e->hnext = NULL;
    g_total -= e->size + e->gz_size;
    g_count--;
    e->detached = true;
    if (e->refcount == 0) entry_free(e);
}

/* Libère les entrées les moins récemment utilisées jusqu'à disposer de `need` octets. */
static void evict_locked(size_t need) {
    while (g_total + need > g_max_total) {
        CacheEntry *victim = NULL;
        for (int b = 0; b < CACHE_BUCKETS; b++)
            for (CacheEntry *e = g_table[b]; e; e = e->hnext)
                if (e->refcount == 0 && (!victim || e->last_used < victim->last_used)) victim = e;
        if (!victim) return;
        lith_log(LOG_DEBUG, "Cache: éviction de %s", victim->path);
        detach_locked(victim);
    }
}

static CacheEntry *find_locked(const char *path) {
    for (CacheEntry *e = g_table[hash_str(path) % CACHE_BUCKETS]; e; e = e->hnext)
        if (strcmp(e->path, path) == 0) return e;
    return NULL;
}

void cache_init(size_t max_total, size_t max_file) {
    pthread_mutex_lock(&g_mu);
    memset(g_table, 0, sizeof(g_table));
    g_total = 0;
    g_count = 0;
    g_max_total = max_total;
    g_max_file = max_file;
    pthread_mutex_unlock(&g_mu);
}

void cache_destroy(void) {
    pthread_mutex_lock(&g_mu);
    for (int b = 0; b < CACHE_BUCKETS; b++) {
        CacheEntry *e = g_table[b];
        while (e) {
            CacheEntry *next = e->hnext;
            e->detached = true;
            if (e->refcount == 0) entry_free(e);
            e = next;
        }
        g_table[b] = NULL;
    }
    g_total = 0;
    g_count = 0;
    pthread_mutex_unlock(&g_mu);
}

CacheEntry *cache_acquire(const char *path, const lith_stat_t *st, bool *hit) {
    *hit = false;
    if (st->size < 0 || (size_t)st->size > g_max_file || g_max_total == 0) return NULL;

    pthread_mutex_lock(&g_mu);
    CacheEntry *e = find_locked(path);
    if (e && e->mtime == st->mtime && (int64_t)e->size == st->size) {
        e->refcount++;
        e->last_used = ++g_tick;
        pthread_mutex_unlock(&g_mu);
        *hit = true;
        return e;
    }
    if (e) {
        lith_log(LOG_DEBUG, "Cache: %s modifié sur le disque, rechargement", path);
        detach_locked(e);
    }
    pthread_mutex_unlock(&g_mu);

    /* Lecture du fichier hors verrou pour ne bloquer personne pendant l'accès disque. */
    FILE *f = lith_fopen(path, "rb");
    if (!f) return NULL;
    size_t size = (size_t)st->size;
    unsigned char *data = (unsigned char *)malloc(size ? size : 1);
    if (!data) { fclose(f); return NULL; }
    size_t got = size ? fread(data, 1, size, f) : 0;
    fclose(f);
    if (got != size) { free(data); return NULL; }

    CacheEntry *ne = (CacheEntry *)calloc(1, sizeof(*ne));
    char *p = (char *)malloc(strlen(path) + 1);
    if (!ne || !p) { free(ne); free(p); free(data); return NULL; }
    memcpy(p, path, strlen(path) + 1);
    ne->path = p;
    ne->data = data;
    ne->size = size;
    ne->mtime = st->mtime;
    ne->refcount = 1;

    pthread_mutex_lock(&g_mu);
    CacheEntry *other = find_locked(path);
    if (other && other->mtime == st->mtime && (int64_t)other->size == st->size) {
        /* Un autre thread a chargé le même fichier entre-temps : on réutilise le sien. */
        other->refcount++;
        other->last_used = ++g_tick;
        pthread_mutex_unlock(&g_mu);
        entry_free(ne);
        *hit = true;
        return other;
    }
    if (other) detach_locked(other);

    evict_locked(size);
    ne->last_used = ++g_tick;
    if (g_total + size <= g_max_total) {
        uint32_t b = hash_str(path) % CACHE_BUCKETS;
        ne->hnext = g_table[b];
        g_table[b] = ne;
        g_total += size;
        g_count++;
    } else {
        ne->detached = true;   /* ne rentre pas : servi une fois puis libéré */
    }
    pthread_mutex_unlock(&g_mu);
    return ne;
}

void cache_release(CacheEntry *e) {
    if (!e) return;
    pthread_mutex_lock(&g_mu);
    e->refcount--;
    if (e->detached && e->refcount == 0) entry_free(e);
    pthread_mutex_unlock(&g_mu);
}

bool cache_ensure_gzip(CacheEntry *e) {
    pthread_mutex_lock(&g_mu);
    bool done = e->gz_done;
    bool usable = e->gz != NULL;
    pthread_mutex_unlock(&g_mu);
    if (done) return usable;

    size_t gz_size = 0;
    unsigned char *gz = lith_gzip(e->data, e->size, &gz_size);

    pthread_mutex_lock(&g_mu);
    if (!e->gz_done) {
        if (gz && gz_size < e->size) {
            e->gz = gz;
            e->gz_size = gz_size;
            if (!e->detached) {
                g_total += gz_size;
                evict_locked(0);
            }
            gz = NULL;
        }
        e->gz_done = true;
    }
    usable = e->gz != NULL;
    pthread_mutex_unlock(&g_mu);
    free(gz);
    return usable;
}

size_t cache_total_bytes(void) {
    pthread_mutex_lock(&g_mu);
    size_t t = g_total;
    pthread_mutex_unlock(&g_mu);
    return t;
}

int cache_entry_count(void) {
    pthread_mutex_lock(&g_mu);
    int c = g_count;
    pthread_mutex_unlock(&g_mu);
    return c;
}
