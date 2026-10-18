/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "daemon.h"
#include "logger.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

int lith_daemonize(void) {
#ifdef _WIN32
    lith_log(LOG_WARN, "Le mode démon n'existe pas sous Windows : LITH reste au premier plan.");
    return -1;
#else
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();                 /* 1. le parent rend la main au terminal */
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    if (setsid() < 0) return -1;        /* 2. nouvelle session, plus de terminal de contrôle */
    signal(SIGHUP, SIG_IGN);

    pid = fork();                       /* 3. ne pourra jamais réacquérir de terminal */
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    umask(022);
    if (chdir("/") != 0) return -1;     /* 4. ne bloque aucun point de montage */

    int fd = open("/dev/null", O_RDWR); /* 5. entrées/sorties standard vers /dev/null */
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
    return 0;
#endif
}

bool lith_pidfile_write(const char *path) {
    if (!path || !path[0]) return true;
    FILE *f = lith_fopen(path, "w");
    if (!f) return false;
    fprintf(f, "%ld\n", lith_getpid());
    fclose(f);
    return true;
}

long lith_pidfile_read(const char *path) {
    FILE *f = lith_fopen(path, "r");
    if (!f) return -1;
    long pid = -1;
    if (fscanf(f, "%ld", &pid) != 1) pid = -1;
    fclose(f);
    return pid > 0 ? pid : -1;
}

void lith_pidfile_remove(const char *path) {
    if (path && path[0]) remove(path);
}

int lith_stop_daemon(const char *pid_path) {
#ifdef _WIN32
    (void)pid_path;
    fprintf(stderr, "`lith stop` n'est disponible que sous Linux/macOS.\n"
                    "Sous Windows : Ctrl+C dans la fenêtre de LITH, ou `taskkill /IM lith.exe`.\n");
    return 1;
#else
    long pid = lith_pidfile_read(pid_path);
    if (pid <= 0) {
        fprintf(stderr, "Aucun fichier PID valide trouvé (%s). LITH tourne-t-il en mode démon ?\n", pid_path);
        return 1;
    }
    if (kill((pid_t)pid, SIGTERM) != 0) {
        if (errno == ESRCH) {
            fprintf(stderr, "Le processus %ld n'existe plus. Suppression du fichier PID obsolète.\n", pid);
            lith_pidfile_remove(pid_path);
            return 0;
        }
        fprintf(stderr, "Impossible d'arrêter le processus %ld (permissions ?).\n", pid);
        return 1;
    }
    printf("Signal d'arrêt envoyé au processus %ld", pid);
    fflush(stdout);
    for (int i = 0; i < 100; i++) {         /* attend jusqu'à 10 secondes */
        if (kill((pid_t)pid, 0) != 0) {
            printf("\nLITH est arrêté.\n");
            lith_pidfile_remove(pid_path);
            return 0;
        }
        if (i % 10 == 0) { printf("."); fflush(stdout); }
        lith_sleep_ms(100);
    }
    printf("\nLe processus ne s'est pas arrêté dans les temps.\n");
    return 1;
#endif
}
