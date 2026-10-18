/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * unit_tests.c — Tests unitaires des fonctions pures de LITH (sans réseau).
 * Lancement : make test
 */

#include "config.h"
#include "http.h"
#include "lith_crypto.h"
#include "logger.h"
#include "proxy.h"
#include "strbuf.h"
#include "tls.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_failed = 0;

#define CHECK(cond) do { \
        g_run++; \
        if (!(cond)) { g_failed++; printf("  ÉCHEC %s:%d : %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

/* Parse une requête écrite en clair (le parseur modifie le buffer). */
static int parse(const char *raw, HttpRequest *req, char *storage, size_t size) {
    snprintf(storage, size, "%s", raw);
    char *end = strstr(storage, "\r\n\r\n");
    size_t hl = end ? (size_t)(end - storage) : strlen(storage);
    memset(req, 0, sizeof(*req));
    return http_parse_request(storage, hl, req);
}

static void test_parser(void) {
    printf("- analyse des requêtes HTTP\n");
    char buf[4096];
    HttpRequest r;

    CHECK(parse("GET /index.html?x=1&y=2 HTTP/1.1\r\nHost: a\r\nUser-Agent: t\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(strcmp(r.method, "GET") == 0);
    CHECK(strcmp(r.path, "/index.html") == 0);
    CHECK(strcmp(r.query, "x=1&y=2") == 0);
    CHECK(r.version_minor == 1);
    CHECK(r.keep_alive);
    CHECK(strcmp(http_header_get(&r, "user-agent"), "t") == 0);
    CHECK(r.content_length == -1);

    CHECK(parse("GET /a%20b/caf%C3%A9.html HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(strcmp(r.path, "/a b/caf\xC3\xA9.html") == 0);
    CHECK(strcmp(r.raw_path, "/a%20b/caf%C3%A9.html") == 0);

    CHECK(parse("GET / HTTP/1.0\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(!r.keep_alive);
    CHECK(parse("GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(r.keep_alive);
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(!r.keep_alive);

    CHECK(parse("HEAD / HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(r.is_head);

    CHECK(parse("POST /x HTTP/1.1\r\nHost: a\r\nContent-Length: 42\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(r.content_length == 42);

    /* Erreurs attendues */
    CHECK(parse("GET / HTTP/1.1\r\n\r\n", &r, buf, sizeof(buf)) == 400);                     /* Host manquant */
    CHECK(parse("GET / HTTP/2.0\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 505);
    CHECK(parse("GET /\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("GET index.html HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\nBad Header: x\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\n folded\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("POST / HTTP/1.1\r\nHost: a\r\nContent-Length: abc\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: gzip\r\n\r\n", &r, buf, sizeof(buf)) == 501);
    CHECK(parse("GET /a%00b HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("GET /a%zz HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 400);
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", &r, buf, sizeof(buf)) == 400);

    CHECK(parse("POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(r.chunked);

    CHECK(parse("OPTIONS * HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(strcmp(r.path, "*") == 0);
    CHECK(parse("GET http://example.com/p?q HTTP/1.1\r\nHost: a\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(strcmp(r.path, "/p") == 0);
}

static void test_paths(void) {
    printf("- sécurité des chemins\n");
    CHECK(http_path_is_safe("/"));
    CHECK(http_path_is_safe("/index.html"));
    CHECK(http_path_is_safe("/a/b/c.txt"));
    CHECK(http_path_is_safe("/...hidden/x"));
    CHECK(http_path_is_safe("/a..b"));
    CHECK(!http_path_is_safe("/../etc/passwd"));
    CHECK(!http_path_is_safe("/a/../../b"));
    CHECK(!http_path_is_safe("/a/.."));
    CHECK(!http_path_is_safe("/a\\..\\b"));
    CHECK(!http_path_is_safe("relative"));

    CHECK(http_path_has_hidden_segment("/.env"));
    CHECK(http_path_has_hidden_segment("/a/.git/config"));
    CHECK(!http_path_has_hidden_segment("/.well-known/acme-challenge/x"));
    CHECK(!http_path_has_hidden_segment("/a/b.c"));
    CHECK(!http_path_has_hidden_segment("/./a"));

    char out[256];
    CHECK(http_url_decode("/a%2Fb", 6, out, sizeof(out)) == 4 && strcmp(out, "/a/b") == 0);
    CHECK(http_url_decode("%4", 2, out, sizeof(out)) == -1);
    http_url_encode_path("a b&c\xC3\xA9.txt", out, sizeof(out));
    CHECK(strcmp(out, "a%20b&c%C3%A9.txt") == 0);
    http_html_escape("<a href=\"x\">'&'</a>", out, sizeof(out));
    CHECK(strcmp(out, "&lt;a href=&quot;x&quot;&gt;&#39;&amp;&#39;&lt;/a&gt;") == 0);
}

static void test_headers_and_mime(void) {
    printf("- en-têtes, types MIME, dates\n");
    CHECK(http_header_has_token("keep-alive, Upgrade", "upgrade"));
    CHECK(http_header_has_token("close", "close"));
    CHECK(!http_header_has_token("closed", "close"));
    CHECK(http_header_has_token("gzip;q=1.0, br", "gzip"));

    CHECK(strcmp(http_mime_type("/a/b.HTML"), "text/html; charset=utf-8") == 0);
    CHECK(strcmp(http_mime_type("x.css"), "text/css; charset=utf-8") == 0);
    CHECK(strcmp(http_mime_type("x.woff2"), "font/woff2") == 0);
    CHECK(strcmp(http_mime_type("noext"), "application/octet-stream") == 0);
    CHECK(strcmp(http_mime_type("dir.v2/file"), "application/octet-stream") == 0);
    CHECK(http_mime_is_compressible("text/html; charset=utf-8"));
    CHECK(http_mime_is_compressible("image/svg+xml"));
    CHECK(!http_mime_is_compressible("image/png"));

    char date[64];
    http_format_date(784111777, date, sizeof(date));
    CHECK(strcmp(date, "Sun, 06 Nov 1994 08:49:37 GMT") == 0);
    CHECK(http_parse_date("Sun, 06 Nov 1994 08:49:37 GMT") == 784111777);
    CHECK(http_parse_date("n'importe quoi") == (time_t)-1);
    CHECK(strcmp(http_status_text(404), "Not Found") == 0);

    char buf[512];
    HttpRequest r;
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\nAccept-Encoding: deflate, gzip;q=0\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(!http_accepts_gzip(&r));
    CHECK(parse("GET / HTTP/1.1\r\nHost: a\r\nAccept-Encoding: gzip, deflate, br\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    CHECK(http_accepts_gzip(&r));
}

static void test_range(void) {
    printf("- requêtes partielles (Range)\n");
    int64_t s, e;
    CHECK(http_parse_range("bytes=0-99", 1000, &s, &e) == 1 && s == 0 && e == 99);
    CHECK(http_parse_range("bytes=500-", 1000, &s, &e) == 1 && s == 500 && e == 999);
    CHECK(http_parse_range("bytes=-100", 1000, &s, &e) == 1 && s == 900 && e == 999);
    CHECK(http_parse_range("bytes=900-5000", 1000, &s, &e) == 1 && e == 999);
    CHECK(http_parse_range("bytes=-5000", 1000, &s, &e) == 1 && s == 0);
    CHECK(http_parse_range("bytes=1000-", 1000, &s, &e) == -1);
    CHECK(http_parse_range("bytes=-0", 1000, &s, &e) == -1);
    CHECK(http_parse_range("bytes=5-1", 1000, &s, &e) == 0);
    CHECK(http_parse_range("bytes=0-1,5-6", 1000, &s, &e) == 0);
    CHECK(http_parse_range("items=0-1", 1000, &s, &e) == 0);
}

static void test_crypto(void) {
    printf("- SHA-1, Base64, CRC-32, WebSocket\n");
    unsigned char d[20];
    char hex[41];
    lith_sha1("abc", 3, d);
    for (int i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", d[i]);
    CHECK(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    lith_sha1("", 0, d);
    for (int i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", d[i]);
    CHECK(strcmp(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
    const char *long_msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    lith_sha1(long_msg, strlen(long_msg), d);
    for (int i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", d[i]);
    CHECK(strcmp(hex, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);

    char b64[64];
    unsigned char raw[64];
    CHECK(lith_base64_encode((const unsigned char *)"foobar", 6, b64, sizeof(b64)) == 8 && strcmp(b64, "Zm9vYmFy") == 0);
    CHECK(lith_base64_encode((const unsigned char *)"fo", 2, b64, sizeof(b64)) == 4 && strcmp(b64, "Zm8=") == 0);
    CHECK(lith_base64_decode("YWRtaW46czNjcmV0", raw, sizeof(raw)) == 12 && strcmp((char *)raw, "admin:s3cret") == 0);
    CHECK(lith_base64_decode("Zm8=", raw, sizeof(raw)) == 2);
    CHECK(lith_base64_decode("Zm8", raw, sizeof(raw)) == -1);
    CHECK(lith_base64_decode("Z=m8", raw, sizeof(raw)) == -1);

    CHECK(lith_crc32(0, "123456789", 9) == 0xCBF43926u);

    char accept[64];
    CHECK(ws_compute_accept("dGhlIHNhbXBsZSBub25jZQ==", accept, sizeof(accept)));
    CHECK(strcmp(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);

    CHECK(lith_secure_equals("abc", "abc"));
    CHECK(!lith_secure_equals("abc", "abd"));
    CHECK(!lith_secure_equals("abc", "abcd"));
}

/* Mini-décompresseur deflate (codes fixes uniquement) pour valider lith_gzip sans zlib. */
typedef struct { const unsigned char *p; size_t len, pos; int bit; } BitReader;

static int br_bit(BitReader *b) {
    if (b->pos >= b->len) return -1;
    int v = (b->p[b->pos] >> b->bit) & 1;
    if (++b->bit == 8) { b->bit = 0; b->pos++; }
    return v;
}
static int br_bits(BitReader *b, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) { int x = br_bit(b); if (x < 0) return -1; v |= x << i; }
    return v;
}
static int br_huff(BitReader *b, int n) {   /* lit n bits, bit de poids fort en premier */
    int v = 0;
    for (int i = 0; i < n; i++) { int x = br_bit(b); if (x < 0) return -1; v = (v << 1) | x; }
    return v;
}
static int decode_litlen(BitReader *b) {
    int c = br_huff(b, 7);
    if (c < 0) return -1;
    if (c <= 0x17) return 256 + c;
    c = (c << 1) | br_bit(b);
    if (c >= 0x30 && c <= 0xBF) return c - 0x30;
    if (c >= 0xC0 && c <= 0xC7) return 280 + (c - 0xC0);
    c = (c << 1) | br_bit(b);
    if (c >= 0x190 && c <= 0x1FF) return 144 + (c - 0x190);
    return -1;
}

static unsigned char *inflate_fixed(const unsigned char *gz, size_t gzlen, size_t *outlen) {
    static const int LB[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
    static const int LE[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
    static const int DB[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
    static const int DE[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
    if (gzlen < 18 || gz[0] != 0x1f || gz[1] != 0x8b) return NULL;
    BitReader b = { gz + 10, gzlen - 18, 0, 0 };
    if (br_bits(&b, 1) != 1 || br_bits(&b, 2) != 1) return NULL;
    size_t cap = 1024, n = 0;
    unsigned char *out = (unsigned char *)malloc(cap);
    for (;;) {
        int sym = decode_litlen(&b);
        if (sym < 0) { free(out); return NULL; }
        if (sym == 256) break;
        if (n + 300 > cap) { cap *= 2; out = (unsigned char *)realloc(out, cap); }
        if (sym < 256) { out[n++] = (unsigned char)sym; continue; }
        int li = sym - 257;
        int len = LB[li] + br_bits(&b, LE[li]);
        int di = br_huff(&b, 5);
        if (di < 0 || di > 29) { free(out); return NULL; }
        int dist = DB[di] + br_bits(&b, DE[di]);
        if ((size_t)dist > n) { free(out); return NULL; }
        for (int k = 0; k < len; k++) { out[n] = out[n - (size_t)dist]; n++; }
    }
    *outlen = n;
    return out;
}

static void check_gzip_roundtrip(const unsigned char *data, size_t len) {
    size_t gl = 0, ol = 0;
    unsigned char *gz = lith_gzip(data, len, &gl);
    CHECK(gz != NULL);
    if (!gz) return;
    unsigned char *back = inflate_fixed(gz, gl, &ol);
    CHECK(back != NULL && ol == len && memcmp(back, data, len) == 0);
    uint32_t crc = (uint32_t)gz[gl - 8] | ((uint32_t)gz[gl - 7] << 8) | ((uint32_t)gz[gl - 6] << 16) | ((uint32_t)gz[gl - 5] << 24);
    CHECK(crc == lith_crc32(0, data, len));
    free(back);
    free(gz);
}

static void test_gzip(void) {
    printf("- compression gzip\n");
    check_gzip_roundtrip((const unsigned char *)"", 0);
    check_gzip_roundtrip((const unsigned char *)"a", 1);
    const char *html = "<html><body><p>Bonjour</p><p>Bonjour</p><p>Bonjour</p></body></html>";
    check_gzip_roundtrip((const unsigned char *)html, strlen(html));

    size_t n = 200000;
    unsigned char *mix = (unsigned char *)malloc(n);
    uint32_t seed = 12345;
    for (size_t i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        mix[i] = (i % 1000 < 500) ? (unsigned char)("lith server "[i % 12]) : (unsigned char)(seed >> 16);
    }
    check_gzip_roundtrip(mix, n);
    memset(mix, 'z', n);
    size_t gl = 0;
    unsigned char *gz = lith_gzip(mix, n, &gl);
    CHECK(gz && gl < n / 50);   /* données très répétitives => forte compression */
    free(gz);
    check_gzip_roundtrip(mix, n);
    free(mix);
}

static void test_config(void) {
    printf("- configuration\n");
    long long v;
    CHECK(config_parse_size("1024", &v) && v == 1024);
    CHECK(config_parse_size("10M", &v) && v == 10LL * 1024 * 1024);
    CHECK(config_parse_size("512k", &v) && v == 512LL * 1024);
    CHECK(config_parse_size("2G", &v) && v == 2LL * 1024 * 1024 * 1024);
    CHECK(config_parse_size("64MB", &v) && v == 64LL * 1024 * 1024);
    CHECK(!config_parse_size("abc", &v));
    CHECK(!config_parse_size("-5", &v));
    CHECK(!config_parse_size("5X", &v));
    bool b;
    CHECK(config_parse_bool("on", &b) && b);
    CHECK(config_parse_bool("0", &b) && !b);
    CHECK(!config_parse_bool("peut-etre", &b));

    LithConfig c;
    char err[256];
    config_set_defaults(&c);
    CHECK(c.port == 8090 && c.gzip && c.use_cache);
    CHECK(config_apply(&c, "PORT", "8080", err, sizeof(err)) && c.port == 8080);
    CHECK(!config_apply(&c, "PORT", "99999", err, sizeof(err)));
    CHECK(!config_apply(&c, "PORT", "abc", err, sizeof(err)));
    CHECK(!config_apply(&c, "INCONNUE", "1", err, sizeof(err)));
    CHECK(config_apply(&c, "PROXY", "/api http://127.0.0.1:3000, localhost:3001", err, sizeof(err)));
    CHECK(c.proxy_count == 1 && c.proxies[0].backend_count == 2);
    CHECK(strcmp(c.proxies[0].prefix, "/api") == 0);
    CHECK(strcmp(c.proxies[0].backends[1].host, "localhost") == 0 && strcmp(c.proxies[0].backends[1].port, "3001") == 0);
    CHECK(config_apply(&c, "PROXY", "/v6/ [::1]:4000", err, sizeof(err)));
    CHECK(strcmp(c.proxies[1].prefix, "/v6") == 0 && strcmp(c.proxies[1].backends[0].host, "::1") == 0);
    CHECK(!config_apply(&c, "PROXY", "/x https://a:1", err, sizeof(err)));
    CHECK(!config_apply(&c, "PROXY", "api http://a:1", err, sizeof(err)));
    CHECK(config_apply(&c, "AUTH", "/admin user:pass", err, sizeof(err)) && c.auth_count == 1);
    CHECK(!config_apply(&c, "AUTH", "/admin sans-deux-points", err, sizeof(err)));
    CHECK(config_apply(&c, "PUBLIC_DIR", "site/", err, sizeof(err)) && strcmp(c.public_dir, "site") == 0);
    CHECK(config_apply(&c, "MAX_BODY_SIZE", "1M", err, sizeof(err)) && c.max_body_size == 1048576);
    CHECK(config_apply(&c, "LOG_LEVEL", "debug", err, sizeof(err)) && c.log_level == LOG_DEBUG);
}

static void test_https_config(void) {
    printf("- configuration HTTPS\n");
    LithConfig c;
    char err[256];
    config_set_defaults(&c);
    CHECK(c.tls_port == 0 && !c.tls_redirect && c.hsts == 0);   /* HTTPS désactivé par défaut */
    CHECK(config_apply(&c, "TLS_PORT", "8443", err, sizeof(err)) && c.tls_port == 8443);
    CHECK(config_apply(&c, "TLS_PORT", "0", err, sizeof(err)) && c.tls_port == 0);
    CHECK(!config_apply(&c, "TLS_PORT", "70000", err, sizeof(err)));
    CHECK(config_apply(&c, "TLS_CERT", "certs/fullchain.pem", err, sizeof(err)) && strcmp(c.tls_cert, "certs/fullchain.pem") == 0);
    CHECK(config_apply(&c, "TLS_KEY", "certs/privkey.pem", err, sizeof(err)) && strcmp(c.tls_key, "certs/privkey.pem") == 0);
    CHECK(config_apply(&c, "TLS_REDIRECT", "on", err, sizeof(err)) && c.tls_redirect);
    CHECK(!config_apply(&c, "TLS_REDIRECT", "parfois", err, sizeof(err)));
    CHECK(config_apply(&c, "HSTS", "31536000", err, sizeof(err)) && c.hsts == 31536000);
    CHECK(!config_apply(&c, "HSTS", "-1", err, sizeof(err)));

    /* Certificat absent : refusé au démarrage, quel que soit le mode de compilation. */
    snprintf(c.public_dir, sizeof(c.public_dir), ".");
    c.tls_port = 8443;
    snprintf(c.tls_cert, sizeof(c.tls_cert), "inexistant-cert.pem");
    snprintf(c.tls_key, sizeof(c.tls_key), "inexistant-key.pem");
    lith_log_set_level((LogLevel)(LOG_ERROR + 1));   /* erreurs attendues : on les masque */
    CHECK(config_validate(&c) >= 1);
    CHECK(!tls_check_files(c.tls_cert, c.tls_key, err, sizeof(err)) && err[0]);
    /* Même port pour HTTP et HTTPS : refusé. */
    c.tls_port = c.port;
    CHECK(config_validate(&c) >= 2);
    /* TLS_REDIRECT sans HTTPS : ignoré. */
    c.tls_port = 0;
    c.tls_redirect = true;
    CHECK(config_validate(&c) == 0 && !c.tls_redirect);
    lith_log_set_level(LOG_INFO);
}

static void test_forwarded_proto(void) {
    printf("- proxy : X-Forwarded-Proto\n");
    char buf[1024];
    HttpRequest r;
    CHECK(parse("GET /api/x HTTP/1.1\r\nHost: site.fr\r\nX-Forwarded-Proto: https\r\n\r\n", &r, buf, sizeof(buf)) == 0);
    LithConn c;
    memset(&c, 0, sizeof(c));
    snprintf(c.ip, sizeof(c.ip), "203.0.113.7");
    BackendConf bc = { "127.0.0.1", "3000" };

    size_t len = 0;
    char *out = proxy_build_request(&c, &r, &bc, &len);
    /* En clair : "http", et l'en-tête envoyé par le client (falsifiable) n'est pas recopié. */
    CHECK(out && strstr(out, "X-Forwarded-Proto: http\r\n") && !strstr(out, "X-Forwarded-Proto: https"));
    CHECK(out && strstr(out, "X-Forwarded-Host: site.fr\r\n") && strstr(out, "X-Forwarded-For: 203.0.113.7\r\n"));
    free(out);

    c.io.tls = (RtlsSession *)&c;   /* marqueur "connexion chiffrée" : jamais déréférencé ici */
    out = proxy_build_request(&c, &r, &bc, &len);
    CHECK(out && strstr(out, "X-Forwarded-Proto: https\r\n"));
    free(out);
}

static void test_strbuf(void) {
    printf("- tampon de texte\n");
    StrBuf sb;
    sb_init(&sb);
    for (int i = 0; i < 1000; i++) sb_printf(&sb, "%d,", i);
    size_t len = 0;
    char *s = sb_take(&sb, &len);
    CHECK(s && len == strlen(s) && strncmp(s, "0,1,2,", 6) == 0 && strstr(s, "999,"));
    free(s);
}

int main(void) {
    printf("Tests unitaires LITH %s\n", LITH_VERSION);
    test_parser();
    test_paths();
    test_headers_and_mime();
    test_range();
    test_crypto();
    test_gzip();
    test_config();
    test_https_config();
    test_forwarded_proto();
    test_strbuf();
    printf("\n%d vérifications, %d échec(s)\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
