/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * daemon.h — Mode arrière-plan (Linux/macOS) et fichier PID.
 */

#ifndef LITH_DAEMON_H
#define LITH_DAEMON_H

#include <stdbool.h>

/* Détache le processus du terminal (double fork). Retourne 0 dans le processus démon. */
int  lith_daemonize(void);

bool lith_pidfile_write(const char *path);
long lith_pidfile_read(const char *path);     /* -1 si absent ou invalide */
void lith_pidfile_remove(const char *path);

/* Envoie SIGTERM au processus du fichier PID et attend son arrêt. Retourne 0 si arrêté. */
int  lith_stop_daemon(const char *pid_path);

#endif /* LITH_DAEMON_H */
