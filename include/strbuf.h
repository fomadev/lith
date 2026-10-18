/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * strbuf.h — Petite chaîne de caractères extensible (pages générées, JSON...).
 */

#ifndef LITH_STRBUF_H
#define LITH_STRBUF_H

#include <stddef.h>
#include <stdbool.h>
#include "platform.h"

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
    bool   failed;   /* une allocation a échoué : le contenu est incomplet */
} StrBuf;

void  sb_init(StrBuf *sb);
void  sb_append(StrBuf *sb, const char *s, size_t n);
void  sb_puts(StrBuf *sb, const char *s);
void  sb_printf(StrBuf *sb, const char *fmt, ...) LITH_PRINTF(2, 3);
/* Rend la chaîne (à libérer avec free) et réinitialise le buffer. */
char *sb_take(StrBuf *sb, size_t *len);
void  sb_free(StrBuf *sb);

#endif /* LITH_STRBUF_H */
