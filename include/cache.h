/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * cache.h — Cache de fichiers en mémoire vive.
 *
 * - N'importe quel fichier sous CACHE_FILE_MAX peut entrer dans le cache.
 * - Une entrée est rechargée automatiquement si le fichier change sur le disque.
 * - Compteur de références : un thread qui envoie un fichier n'en bloque aucun autre.
 * - Éviction LRU (le moins récemment utilisé part en premier) au-delà de CACHE_MAX_SIZE.
 * - La version gzip est calculée une seule fois, à la première demande.
 */

#ifndef LITH_CACHE_H
#define LITH_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "platform.h"

typedef struct CacheEntry {
    char          *path;
    unsigned char *data;
    size_t         size;
    time_t         mtime;
    unsigned char *gz;          /* NULL tant que non calculé */
    size_t         gz_size;
    bool           gz_done;     /* compression tentée (même si inutile) */
    int            refcount;
    bool           detached;    /* retirée de la table, libérée au dernier release */
    uint64_t       last_used;
    struct CacheEntry *hnext;   /* chaînage de la table de hachage */
} CacheEntry;

void        cache_init(size_t max_total, size_t max_file);
void        cache_destroy(void);

/* Retourne une entrée à jour (chargée si nécessaire) ou NULL. *hit indique si elle était déjà en RAM. */
CacheEntry *cache_acquire(const char *path, const lith_stat_t *st, bool *hit);
void        cache_release(CacheEntry *e);

/* Calcule la version gzip si besoin. Retourne true si e->gz est utilisable. */
bool        cache_ensure_gzip(CacheEntry *e);

size_t      cache_total_bytes(void);
int         cache_entry_count(void);

#endif /* LITH_CACHE_H */
