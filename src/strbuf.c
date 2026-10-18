/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void sb_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    sb->failed = false;
}

static bool sb_reserve(StrBuf *sb, size_t extra) {
    if (sb->failed) return false;
    if (sb->len + extra + 1 <= sb->cap) return true;
    size_t cap = sb->cap ? sb->cap : 1024;
    while (cap < sb->len + extra + 1) cap *= 2;
    char *p = (char *)realloc(sb->data, cap);
    if (!p) {
        sb->failed = true;
        return false;
    }
    sb->data = p;
    sb->cap = cap;
    return true;
}

void sb_append(StrBuf *sb, const char *s, size_t n) {
    if (!sb_reserve(sb, n)) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_puts(StrBuf *sb, const char *s) {
    sb_append(sb, s, strlen(s));
}

void sb_printf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) {
        sb_append(sb, tmp, (size_t)n);
        return;
    }
    if (!sb_reserve(sb, (size_t)n)) return;
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t)n;
}

char *sb_take(StrBuf *sb, size_t *len) {
    if (sb->failed) {
        sb_free(sb);
        if (len) *len = 0;
        return NULL;
    }
    if (!sb->data) sb_append(sb, "", 0);
    char *d = sb->data;
    if (len) *len = sb->len;
    sb_init(sb);
    return d;
}

void sb_free(StrBuf *sb) {
    free(sb->data);
    sb_init(sb);
}
