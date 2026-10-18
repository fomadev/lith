/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "logger.h"
#include "platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>

#define CLR_RESET "\x1b[0m"
#define CLR_DEBUG "\x1b[90m"
#define CLR_INFO  "\x1b[32m"
#define CLR_WARN  "\x1b[33m"
#define CLR_ERROR "\x1b[31m"

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int g_min_level = LOG_INFO;
static FILE *g_out = NULL;      /* NULL => stdout */
static bool g_colors = false;

int lith_log_init(LogLevel min_level, const char *file_path) {
    pthread_mutex_lock(&g_log_mutex);
    g_min_level = min_level;
    if (g_out) { fclose(g_out); g_out = NULL; }

    int rc = 0;
    if (file_path && file_path[0]) {
        g_out = lith_fopen(file_path, "a");
        if (!g_out) rc = -1;
    }
    g_colors = (g_out == NULL) && lith_stdout_is_tty();
#ifdef _WIN32
    if (g_colors) {
        /* Active les séquences ANSI dans la console Windows 10+ */
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            if (!SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */))
                g_colors = false;
        }
        SetConsoleOutputCP(CP_UTF8);
    }
#endif
    pthread_mutex_unlock(&g_log_mutex);
    return rc;
}

void lith_log_set_level(LogLevel min_level) {
    pthread_mutex_lock(&g_log_mutex);
    g_min_level = min_level;
    pthread_mutex_unlock(&g_log_mutex);
}

void lith_log_close(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_out) { fclose(g_out); g_out = NULL; }
    pthread_mutex_unlock(&g_log_mutex);
}

const char *lith_log_level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
    }
    return "LOG";
}

bool lith_log_level_parse(const char *s, LogLevel *out) {
    if (!s) return false;
    if (strcasecmp(s, "debug") == 0) { *out = LOG_DEBUG; return true; }
    if (strcasecmp(s, "info") == 0)  { *out = LOG_INFO;  return true; }
    if (strcasecmp(s, "warn") == 0 || strcasecmp(s, "warning") == 0) { *out = LOG_WARN; return true; }
    if (strcasecmp(s, "error") == 0) { *out = LOG_ERROR; return true; }
    return false;
}

static void timestamp(char *buf, size_t size) {
    struct tm tm;
    lith_localtime(time(NULL), &tm);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm);
}

void lith_log(LogLevel level, const char *format, ...) {
    if ((int)level < atomic_load(&g_min_level)) return;

    char msg[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    char ts[32];
    timestamp(ts, sizeof(ts));

    const char *color = CLR_RESET;
    switch (level) {
        case LOG_DEBUG: color = CLR_DEBUG; break;
        case LOG_INFO:  color = CLR_INFO;  break;
        case LOG_WARN:  color = CLR_WARN;  break;
        case LOG_ERROR: color = CLR_ERROR; break;
    }

    pthread_mutex_lock(&g_log_mutex);
    FILE *out = g_out ? g_out : stdout;
    if (g_colors)
        fprintf(out, "[%s] %s%-5s%s %s\n", ts, color, lith_log_level_name(level), CLR_RESET, msg);
    else
        fprintf(out, "[%s] %-5s %s\n", ts, lith_log_level_name(level), msg);
    fflush(out);
    pthread_mutex_unlock(&g_log_mutex);
}

void lith_log_access(const char *line) {
    char ts[32];
    timestamp(ts, sizeof(ts));
    pthread_mutex_lock(&g_log_mutex);
    FILE *out = g_out ? g_out : stdout;
    fprintf(out, "[%s] %s\n", ts, line);
    fflush(out);
    pthread_mutex_unlock(&g_log_mutex);
}
