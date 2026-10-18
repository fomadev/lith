/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "lith_crypto.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ==========================================================================
 * SHA-1 (FIPS 180-1) — utilisé uniquement pour la clé Sec-WebSocket-Accept.
 * ========================================================================== */

#define ROL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static void sha1_block(uint32_t h[5], const unsigned char *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = ROL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6; }
        uint32_t t = ROL32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL32(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void lith_sha1(const void *data, size_t len, unsigned char out[20]) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    const unsigned char *p = (const unsigned char *)data;
    size_t remaining = len;
    while (remaining >= 64) {
        sha1_block(h, p);
        p += 64;
        remaining -= 64;
    }
    unsigned char tail[128];
    memset(tail, 0, sizeof(tail));
    memcpy(tail, p, remaining);
    tail[remaining] = 0x80;
    size_t tail_len = (remaining < 56) ? 64 : 128;
    uint64_t bits = (uint64_t)len * 8u;
    for (int i = 0; i < 8; i++)
        tail[tail_len - 1 - i] = (unsigned char)(bits >> (8 * i));
    sha1_block(h, tail);
    if (tail_len == 128) sha1_block(h, tail + 64);
    for (int i = 0; i < 5; i++) {
        out[i * 4]     = (unsigned char)(h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)h[i];
    }
}

/* ==========================================================================
 * Base64 (RFC 4648)
 * ========================================================================== */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t lith_base64_encode(const unsigned char *src, size_t len, char *dst, size_t dst_size) {
    size_t need = ((len + 2) / 3) * 4;
    if (dst_size < need + 1) return 0;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (i + 1 < len) v |= (uint32_t)src[i + 1] << 8;
        if (i + 2 < len) v |= src[i + 2];
        dst[o++] = B64[(v >> 18) & 63];
        dst[o++] = B64[(v >> 12) & 63];
        dst[o++] = (i + 1 < len) ? B64[(v >> 6) & 63] : '=';
        dst[o++] = (i + 2 < len) ? B64[v & 63] : '=';
    }
    dst[o] = '\0';
    return o;
}

static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

long lith_base64_decode(const char *src, unsigned char *dst, size_t dst_size) {
    size_t len = strlen(src);
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t')) len--;
    if (len % 4 != 0) return -1;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 4) {
        int v[4];
        int pad = 0;
        for (int j = 0; j < 4; j++) {
            char c = src[i + j];
            if (c == '=') {
                if (i + 4 != len || j < 2) return -1;
                v[j] = 0;
                pad++;
            } else {
                if (pad) return -1;
                v[j] = b64_val(c);
                if (v[j] < 0) return -1;
            }
        }
        uint32_t w = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) | ((uint32_t)v[2] << 6) | (uint32_t)v[3];
        int bytes = 3 - pad;
        if (o + (size_t)bytes >= dst_size) return -1;
        dst[o++] = (unsigned char)(w >> 16);
        if (bytes > 1) dst[o++] = (unsigned char)(w >> 8);
        if (bytes > 2) dst[o++] = (unsigned char)w;
    }
    dst[o] = '\0';
    return (long)o;
}

bool lith_secure_equals(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    size_t n = la > lb ? la : lb;
    unsigned char diff = (unsigned char)(la != lb);
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = i < la ? (unsigned char)a[i] : 0;
        unsigned char cb = i < lb ? (unsigned char)b[i] : 0;
        diff |= (unsigned char)(ca ^ cb);
    }
    return diff == 0;
}

/* ==========================================================================
 * CRC-32 (polynôme 0xEDB88320, celui de gzip/zip)
 * ========================================================================== */

static uint32_t g_crc_table[256];
static pthread_once_t g_crc_once = PTHREAD_ONCE_INIT;

static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crc_table[i] = c;
    }
}

uint32_t lith_crc32(uint32_t crc, const void *data, size_t len) {
    pthread_once(&g_crc_once, crc_init);
    const unsigned char *p = (const unsigned char *)data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) crc = g_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ==========================================================================
 * Compression deflate (LZ77 + codes de Huffman fixes) emballée en gzip.
 *
 * Volontairement simple : un seul bloc à codes fixes (BTYPE=01). Le taux
 * obtenu sur du HTML/CSS/JS est généralement de 60 à 75 % de réduction,
 * sans aucune bibliothèque externe.
 * ========================================================================== */

#define WSIZE       32768
#define WMASK       (WSIZE - 1)
#define HASH_BITS   15
#define HASH_SIZE   (1 << HASH_BITS)
#define MAX_CHAIN   64
#define MIN_MATCH   3
#define MAX_MATCH   258

typedef struct {
    unsigned char *buf;
    size_t len;
    uint32_t bits;
    int nbits;
} BitWriter;

static inline void bw_put(BitWriter *w, uint32_t value, int n) {
    w->bits |= value << w->nbits;
    w->nbits += n;
    while (w->nbits >= 8) {
        w->buf[w->len++] = (unsigned char)(w->bits & 0xFF);
        w->bits >>= 8;
        w->nbits -= 8;
    }
}

static inline uint32_t reverse_bits(uint32_t code, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) { r = (r << 1) | (code & 1); code >>= 1; }
    return r;
}

/* Les codes de Huffman s'écrivent bit de poids fort d'abord : on les inverse. */
static inline void put_litlen(BitWriter *w, int v) {
    if (v <= 143)      bw_put(w, reverse_bits(0x30u + (uint32_t)v, 8), 8);
    else if (v <= 255) bw_put(w, reverse_bits(0x190u + (uint32_t)(v - 144), 9), 9);
    else if (v <= 279) bw_put(w, reverse_bits((uint32_t)(v - 256), 7), 7);
    else               bw_put(w, reverse_bits(0xC0u + (uint32_t)(v - 280), 8), 8);
}

static const uint16_t LEN_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                       35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t LEN_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                       3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                        8193, 12289, 16385, 24577 };
static const uint8_t DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void put_match(BitWriter *w, int length, int distance) {
    int li = 28;
    while (LEN_BASE[li] > length) li--;
    put_litlen(w, 257 + li);
    if (LEN_EXTRA[li]) bw_put(w, (uint32_t)(length - LEN_BASE[li]), LEN_EXTRA[li]);

    int di = 29;
    while (DIST_BASE[di] > distance) di--;
    bw_put(w, reverse_bits((uint32_t)di, 5), 5);
    if (DIST_EXTRA[di]) bw_put(w, (uint32_t)(distance - DIST_BASE[di]), DIST_EXTRA[di]);
}

static inline uint32_t hash3(const unsigned char *p) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return (v * 2654435761u) >> (32 - HASH_BITS);
}

unsigned char *lith_gzip(const unsigned char *in, size_t len, size_t *out_len) {
    /* Pire cas : 9 bits par octet en littéral + en-têtes. */
    size_t cap = len + len / 8 + 64;
    unsigned char *out = (unsigned char *)malloc(cap);
    int32_t *head = (int32_t *)malloc(sizeof(int32_t) * HASH_SIZE);
    int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * WSIZE);
    if (!out || !head || !prev) {
        free(out); free(head); free(prev);
        return NULL;
    }
    for (int i = 0; i < HASH_SIZE; i++) head[i] = -1;

    /* En-tête gzip : magic, méthode deflate, pas de drapeaux, mtime 0, OS inconnu */
    static const unsigned char GZ_HEADER[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff };
    memcpy(out, GZ_HEADER, 10);

    BitWriter w = { out, 10, 0, 0 };
    bw_put(&w, 1, 1);   /* BFINAL = 1 */
    bw_put(&w, 1, 2);   /* BTYPE  = 01 (Huffman fixe) */

    size_t i = 0;
    while (i < len) {
        int best_len = 0, best_dist = 0;
        if (i + MIN_MATCH <= len) {
            uint32_t h = hash3(in + i);
            int32_t pos = head[h];
            int chain = MAX_CHAIN;
            size_t max_len = len - i;
            if (max_len > MAX_MATCH) max_len = MAX_MATCH;
            while (pos >= 0 && i - (size_t)pos <= WSIZE && chain-- > 0) {
                const unsigned char *a = in + pos;
                const unsigned char *b = in + i;
                if (a[best_len] == b[best_len] && a[0] == b[0]) {
                    size_t l = 0;
                    while (l < max_len && a[l] == b[l]) l++;
                    if ((int)l > best_len) {
                        best_len = (int)l;
                        best_dist = (int)(i - (size_t)pos);
                        if (l == max_len) break;
                    }
                }
                int32_t next = prev[pos & WMASK];
                if (next >= pos) break;   /* entrée écrasée : fin de chaîne */
                pos = next;
            }
            prev[i & WMASK] = head[h];
            head[h] = (int32_t)i;
        }

        if (best_len >= MIN_MATCH) {
            put_match(&w, best_len, best_dist);
            for (int k = 1; k < best_len; k++) {
                size_t j = i + (size_t)k;
                if (j + MIN_MATCH <= len) {
                    uint32_t h = hash3(in + j);
                    prev[j & WMASK] = head[h];
                    head[h] = (int32_t)j;
                }
            }
            i += (size_t)best_len;
        } else {
            put_litlen(&w, in[i]);
            i++;
        }
    }
    put_litlen(&w, 256);                    /* fin de bloc */
    if (w.nbits > 0) bw_put(&w, 0, 8 - w.nbits);

    uint32_t crc = lith_crc32(0, in, len);
    uint32_t isize = (uint32_t)len;
    for (int k = 0; k < 4; k++) out[w.len++] = (unsigned char)(crc >> (8 * k));
    for (int k = 0; k < 4; k++) out[w.len++] = (unsigned char)(isize >> (8 * k));

    free(head);
    free(prev);
    *out_len = w.len;
    return out;
}
