/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #define PSAPI_VERSION 2
    #include <psapi.h>
#else
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include <signal.h>
    #include <poll.h>
#endif

/* ==========================================================================
 * Réseau
 * ========================================================================== */

int lith_net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    signal(SIGPIPE, SIG_IGN); /* une écriture vers un client parti ne doit pas tuer le serveur */
    return 0;
#endif
}

void lith_net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void lith_socket_close(socket_t s) {
    if (s == LITH_INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

int lith_socket_set_recv_timeout(socket_t s, int ms) {
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

int lith_socket_set_send_timeout(socket_t s, int ms) {
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

int lith_send_all(socket_t s, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        int chunk = len > (1u << 30) ? (1 << 30) : (int)len;
#ifdef _WIN32
        int n = send(s, p, chunk, 0);
#else
    #ifdef MSG_NOSIGNAL
        ssize_t n = send(s, p, (size_t)chunk, MSG_NOSIGNAL);
    #else
        ssize_t n = send(s, p, (size_t)chunk, 0);
    #endif
        if (n < 0 && errno == EINTR) continue;
#endif
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

long lith_recv(socket_t s, void *buf, size_t len) {
    int chunk = len > (1u << 30) ? (1 << 30) : (int)len;
#ifdef _WIN32
    int n = recv(s, (char *)buf, chunk, 0);
    return n < 0 ? -1 : n;
#else
    for (;;) {
        ssize_t n = recv(s, buf, (size_t)chunk, 0);
        if (n < 0 && errno == EINTR) continue;
        return n < 0 ? -1 : (long)n;
    }
#endif
}

bool lith_last_error_is_timeout(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAETIMEDOUT;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

void lith_set_last_error(bool timeout) {
#ifdef _WIN32
    WSASetLastError(timeout ? WSAETIMEDOUT : WSAECONNRESET);
#else
    errno = timeout ? EAGAIN : ECONNRESET;
#endif
}

static void set_nonblocking(socket_t s, bool on) {
#ifdef _WIN32
    u_long mode = on ? 1 : 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return;
    fcntl(s, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

/* Connexion TCP sortante avec délai maximum (utilisée par le proxy et les health checks). */
int lith_connect_tcp(const char *host, const char *port, int timeout_ms, socket_t *out) {
    struct addrinfo hints, *res = NULL, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) return -1;

    int rc = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        socket_t s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == LITH_INVALID_SOCKET) continue;

        set_nonblocking(s, true);
        int r = connect(s, ai->ai_addr, (int)ai->ai_addrlen);
        bool in_progress;
#ifdef _WIN32
        in_progress = (r != 0 && WSAGetLastError() == WSAEWOULDBLOCK);
#else
        in_progress = (r != 0 && errno == EINPROGRESS);
#endif
        if (r == 0) {
            set_nonblocking(s, false);
            *out = s;
            rc = 0;
            break;
        }
        if (in_progress) {
            fd_set wfds, efds;
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            FD_SET(s, &wfds);
            FD_SET(s, &efds);
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select((int)s + 1, NULL, &wfds, &efds, &tv) > 0 && FD_ISSET(s, &wfds)) {
                int err = 0;
                socklen_t elen = sizeof(err);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &elen);
                if (err == 0) {
                    set_nonblocking(s, false);
                    *out = s;
                    rc = 0;
                    break;
                }
            }
        }
        lith_socket_close(s);
    }
    freeaddrinfo(res);
    return rc;
}

bool lith_ip_is_loopback(const char *ip) {
    if (!ip) return false;
    return strncmp(ip, "127.", 4) == 0 || strcmp(ip, "::1") == 0 ||
           strncmp(ip, "::ffff:127.", 11) == 0;
}

/* ==========================================================================
 * Fichiers
 * ========================================================================== */

#ifdef _WIN32
/* Conversion UTF-8 -> UTF-16 pour les API "W" de Windows (gère les accents). */
static wchar_t *utf8_to_wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static char *wide_to_utf8(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *s = (char *)malloc((size_t)n);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}
#endif

int lith_stat(const char *path, lith_stat_t *st) {
    memset(st, 0, sizeof(*st));
#ifdef _WIN32
    wchar_t *w = utf8_to_wide(path);
    if (!w) return -1;
    struct _stat64 s;
    int r = _wstat64(w, &s);
    free(w);
    if (r != 0) return -1;
    st->size = (int64_t)s.st_size;
    st->mtime = (time_t)s.st_mtime;
    st->is_dir = (s.st_mode & _S_IFDIR) != 0;
    st->is_file = (s.st_mode & _S_IFREG) != 0;
#else
    struct stat s;
    if (stat(path, &s) != 0) return -1;
    st->size = (int64_t)s.st_size;
    st->mtime = s.st_mtime;
    st->is_dir = S_ISDIR(s.st_mode);
    st->is_file = S_ISREG(s.st_mode);
#endif
    return 0;
}

FILE *lith_fopen(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t *wp = utf8_to_wide(path);
    wchar_t *wm = utf8_to_wide(mode);
    FILE *f = (wp && wm) ? _wfopen(wp, wm) : NULL;
    free(wp);
    free(wm);
    return f;
#else
    return fopen(path, mode);
#endif
}

int lith_fseek64(FILE *f, int64_t offset) {
#ifdef _WIN32
    return _fseeki64(f, offset, SEEK_SET);
#else
    return fseeko(f, (off_t)offset, SEEK_SET);
#endif
}

bool lith_mkdir(const char *path) {
#ifdef _WIN32
    wchar_t *w = utf8_to_wide(path);
    if (!w) return false;
    int r = _wmkdir(w);
    free(w);
    return r == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool lith_path_is_absolute(const char *path) {
    if (!path || !path[0]) return false;
#ifdef _WIN32
    if (path[0] == '\\' || path[0] == '/') return true;
    return path[1] == ':' && (path[2] == '\\' || path[2] == '/');
#else
    return path[0] == '/';
#endif
}

int lith_getcwd(char *buf, size_t size) {
#ifdef _WIN32
    return _getcwd(buf, (int)size) ? 0 : -1;
#else
    return getcwd(buf, size) ? 0 : -1;
#endif
}

struct lith_dir {
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAW data;
    bool first;
    char *current;
#else
    DIR *dir;
#endif
};

lith_dir_t *lith_dir_open(const char *path) {
    lith_dir_t *d = (lith_dir_t *)calloc(1, sizeof(*d));
    if (!d) return NULL;
#ifdef _WIN32
    char pattern[LITH_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    wchar_t *w = utf8_to_wide(pattern);
    if (!w) { free(d); return NULL; }
    d->handle = FindFirstFileW(w, &d->data);
    free(w);
    if (d->handle == INVALID_HANDLE_VALUE) { free(d); return NULL; }
    d->first = true;
#else
    d->dir = opendir(path);
    if (!d->dir) { free(d); return NULL; }
#endif
    return d;
}

const char *lith_dir_next(lith_dir_t *d) {
    if (!d) return NULL;
#ifdef _WIN32
    for (;;) {
        if (!d->first) {
            if (!FindNextFileW(d->handle, &d->data)) return NULL;
        }
        d->first = false;
        free(d->current);
        d->current = wide_to_utf8(d->data.cFileName);
        if (!d->current) continue;
        if (strcmp(d->current, ".") == 0 || strcmp(d->current, "..") == 0) continue;
        return d->current;
    }
#else
    struct dirent *e;
    while ((e = readdir(d->dir)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        return e->d_name;
    }
    return NULL;
#endif
}

void lith_dir_close(lith_dir_t *d) {
    if (!d) return;
#ifdef _WIN32
    FindClose(d->handle);
    free(d->current);
#else
    closedir(d->dir);
#endif
    free(d);
}

/* ==========================================================================
 * Temps
 * ========================================================================== */

void lith_gmtime(time_t t, struct tm *out) {
#ifdef _WIN32
    gmtime_s(out, &t);
#else
    gmtime_r(&t, out);
#endif
}

void lith_localtime(time_t t, struct tm *out) {
#ifdef _WIN32
    localtime_s(out, &t);
#else
    localtime_r(&t, out);
#endif
}

time_t lith_timegm(struct tm *tm) {
#ifdef _WIN32
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

uint64_t lith_now_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

void lith_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
#endif
}

/* ==========================================================================
 * Système
 * ========================================================================== */

int lith_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

uint64_t lith_rss_bytes(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (uint64_t)pmc.WorkingSetSize;
    return 0;
#elif defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long pages_total = 0, pages_rss = 0;
    int ok = fscanf(f, "%lu %lu", &pages_total, &pages_rss);
    fclose(f);
    if (ok != 2) return 0;
    return (uint64_t)pages_rss * (uint64_t)sysconf(_SC_PAGESIZE);
#else
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return 0;
    #ifdef __APPLE__
    return (uint64_t)ru.ru_maxrss;          /* octets sur macOS (pic) */
    #else
    return (uint64_t)ru.ru_maxrss * 1024u;  /* kilo-octets ailleurs (pic) */
    #endif
#endif
}

long lith_getpid(void) {
#ifdef _WIN32
    return (long)GetCurrentProcessId();
#else
    return (long)getpid();
#endif
}

bool lith_stdout_is_tty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}
