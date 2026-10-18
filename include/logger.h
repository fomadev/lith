/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * logger.h — Journalisation thread-safe (console colorée ou fichier).
 */

#ifndef LITH_LOGGER_H
#define LITH_LOGGER_H

#include <stdbool.h>
#include "platform.h"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} LogLevel;

/* file_path NULL ou vide => sortie standard. Retourne -1 si le fichier ne peut pas être ouvert. */
int  lith_log_init(LogLevel min_level, const char *file_path);
void lith_log_set_level(LogLevel min_level);
void lith_log_close(void);

void lith_log(LogLevel level, const char *format, ...) LITH_PRINTF(2, 3);

/* Ligne de journal d'accès (une par requête), sans préfixe de niveau. */
void lith_log_access(const char *line);

bool lith_log_level_parse(const char *s, LogLevel *out);
const char *lith_log_level_name(LogLevel level);

#endif /* LITH_LOGGER_H */
