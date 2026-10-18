/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * platform.h — Couche d'abstraction système (Windows / Linux / macOS).
 *
 * Tout ce qui diffère d'un OS à l'autre (sockets, fichiers, temps, mémoire)
 * passe par ici. Le reste du code n'utilise jamais directement Winsock ou
 * les appels POSIX spécifiques.
 */

#ifndef LITH_PLATFORM_H
#define LITH_PLATFORM_H

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef SOCKET socket_t;
    #define LITH_INVALID_SOCKET INVALID_SOCKET
    #define LITH_PATH_SEP '\\'
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    typedef int socket_t;
    #define LITH_INVALID_SOCKET (-1)
    #define LITH_PATH_SEP '/'
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#define LITH_PATH_MAX 1024

/* Vérification des formats printf par le compilateur (MinGW utilise la variante GNU). */
#if defined(__MINGW32__)
    #define LITH_PRINTF(fmt, args) __attribute__((format(gnu_printf, fmt, args)))
#elif defined(__GNUC__)
    #define LITH_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
    #define LITH_PRINTF(fmt, args)
#endif

/* Informations sur un fichier, indépendantes de l'OS (tailles 64 bits). */
typedef struct {
    int64_t size;
    time_t  mtime;
    bool    is_dir;
    bool    is_file;
} lith_stat_t;

/* ---- Réseau ---- */
int   lith_net_init(void);
void  lith_net_cleanup(void);
void  lith_socket_close(socket_t s);
int   lith_socket_set_recv_timeout(socket_t s, int ms);
int   lith_socket_set_send_timeout(socket_t s, int ms);
int   lith_send_all(socket_t s, const void *buf, size_t len);
long  lith_recv(socket_t s, void *buf, size_t len);
bool  lith_last_error_is_timeout(void);
void  lith_set_last_error(bool timeout);   /* pour les couches au-dessus de recv (TLS) */
int   lith_connect_tcp(const char *host, const char *port, int timeout_ms, socket_t *out);
bool  lith_ip_is_loopback(const char *ip);

/* ---- Fichiers (chemins UTF-8, y compris sous Windows) ---- */
int   lith_stat(const char *path, lith_stat_t *st);
FILE *lith_fopen(const char *path, const char *mode);
int   lith_fseek64(FILE *f, int64_t offset);
bool  lith_mkdir(const char *path);
bool  lith_path_is_absolute(const char *path);
int   lith_getcwd(char *buf, size_t size);

/* Parcours de répertoire (noms UTF-8). */
typedef struct lith_dir lith_dir_t;
lith_dir_t *lith_dir_open(const char *path);
const char *lith_dir_next(lith_dir_t *d);   /* NULL à la fin ; ignore "." et ".." */
void        lith_dir_close(lith_dir_t *d);

/* ---- Temps ---- */
void     lith_gmtime(time_t t, struct tm *out);
void     lith_localtime(time_t t, struct tm *out);
time_t   lith_timegm(struct tm *tm);
uint64_t lith_now_ms(void);          /* horloge monotone en millisecondes */
void     lith_sleep_ms(int ms);

/* ---- Système ---- */
int      lith_cpu_count(void);
uint64_t lith_rss_bytes(void);       /* mémoire résidente du processus (0 si inconnue) */
long     lith_getpid(void);
bool     lith_stdout_is_tty(void);

#endif /* LITH_PLATFORM_H */
