/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * metrics.h — Statistiques en temps réel (/__lith/status et /__lith/metrics).
 */

#ifndef LITH_METRICS_H
#define LITH_METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void metrics_init(int worker_threads);
void metrics_conn_open(void);
void metrics_conn_close(void);
void metrics_record_request(int status, long long bytes, uint64_t duration_ms);
void metrics_cache(bool hit);
void metrics_rate_limited(void);
void metrics_proxy_error(void);

/* Documents générés (alloués avec malloc, à libérer par l'appelant). */
char *metrics_status_json(size_t *len);
char *metrics_prometheus(size_t *len);

#endif /* LITH_METRICS_H */
