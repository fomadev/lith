/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * lith_crypto.h — Primitives intégrées, sans dépendance externe :
 * SHA-1 (poignée de main WebSocket), Base64, CRC-32 et compression gzip.
 */

#ifndef LITH_CRYPTO_H
#define LITH_CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void lith_sha1(const void *data, size_t len, unsigned char out[20]);

/* Retourne la longueur écrite (sans le '\0'), ou 0 si dst est trop petit. */
size_t lith_base64_encode(const unsigned char *src, size_t len, char *dst, size_t dst_size);
/* Retourne la longueur décodée, ou -1 si l'entrée est invalide / dst trop petit. */
long   lith_base64_decode(const char *src, unsigned char *dst, size_t dst_size);

uint32_t lith_crc32(uint32_t crc, const void *data, size_t len);

/* Compresse au format gzip (RFC 1952 / deflate RFC 1951). Résultat alloué avec malloc. */
unsigned char *lith_gzip(const unsigned char *in, size_t len, size_t *out_len);

/* Comparaison en temps constant (évite les attaques temporelles sur les mots de passe). */
bool lith_secure_equals(const char *a, const char *b);

#endif /* LITH_CRYPTO_H */
