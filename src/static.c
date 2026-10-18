/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * static.c — Service des fichiers du dossier public.
 *
 * Ordre de traitement d'un fichier :
 *   1. validation conditionnelle (ETag / Last-Modified -> 304)
 *   2. injection du script de rechargement (LIVE_RELOAD, pages HTML)
 *   3. requêtes partielles (Range -> 206), utiles pour la vidéo
 *   4. cache mémoire + compression gzip
 *   5. sinon, envoi direct depuis le disque par blocs (sendfile sous Linux)
 */

#include "handlers.h"
#include "config.h"
#include "cache.h"
#include "lith_crypto.h"
#include "logger.h"
#include "metrics.h"
#include "strbuf.h"
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#ifdef __linux__
    #include <sys/sendfile.h>
#endif

#define STREAM_CHUNK        (64 * 1024)
#define GZIP_MIN_SIZE       256
#define GZIP_MAX_ON_THE_FLY (8 * 1024 * 1024)
#define LIVERELOAD_MAX_HTML (16 * 1024 * 1024)
#define AUTOINDEX_MAX_ITEMS 10000

static void serve_file(LithConn *c, HttpRequest *req, const char *fs_path, const lith_stat_t *st);

/* ---------------------------------------------------------------------- */

static void make_etag(const lith_stat_t *st, char *out, size_t size) {
    snprintf(out, size, "W/\"%llx-%llx\"", (unsigned long long)st->size, (unsigned long long)st->mtime);
}

/* Compare une liste "If-None-Match" à notre ETag (comparaison faible). */
static bool etag_list_matches(const char *list, const char *etag) {
    const char *e = (strncmp(etag, "W/", 2) == 0) ? etag + 2 : etag;
    size_t elen = strlen(e);
    const char *p = list;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (*p == '*') return true;
        if (strncmp(p, "W/", 2) == 0) p += 2;
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *stop = p;
        while (stop > start && (stop[-1] == ' ' || stop[-1] == '\t')) stop--;
        if ((size_t)(stop - start) == elen && strncmp(start, e, elen) == 0) return true;
    }
    return false;
}

static void add_file_headers(RespHeaders *h, const char *mime, const char *etag, const char *lastmod) {
    resp_add(h, "Content-Type", "%s", mime);
    resp_add(h, "Last-Modified", "%s", lastmod);
    resp_add(h, "ETag", "%s", etag);
    if (g_config.cache_control[0]) resp_add(h, "Cache-Control", "%s", g_config.cache_control);
    if (g_config.gzip && http_mime_is_compressible(mime)) resp_add(h, "Vary", "Accept-Encoding");
}

/* Envoie `length` octets du fichier à partir de `offset`. */
static int stream_file(LithConn *c, FILE *f, int64_t offset, int64_t length) {
    if (c->req && c->req->is_head) return 0;
    if (length <= 0) return 0;

#ifdef __linux__
    if (!conn_is_https(c)) {   /* sendfile() envoie le fichier brut : seulement en clair */
        int fd = fileno(f);
        off_t off = (off_t)offset;
        int64_t left = length;
        while (left > 0) {
            size_t chunk = left > (1 << 30) ? (size_t)(1 << 30) : (size_t)left;
            ssize_t n = sendfile(c->io.sock, fd, &off, chunk);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { c->force_close = true; return -1; }
            left -= n;
            c->bytes_sent += n;
        }
        return 0;
    }
#endif
    if (lith_fseek64(f, offset) != 0) { c->force_close = true; return -1; }
    char *buf = (char *)malloc(STREAM_CHUNK);
    if (!buf) { c->force_close = true; return -1; }
    int64_t left = length;
    int rc = 0;
    while (left > 0) {
        size_t want = left > STREAM_CHUNK ? STREAM_CHUNK : (size_t)left;
        size_t got = fread(buf, 1, want, f);
        if (got == 0) { c->force_close = true; rc = -1; break; }
        if (resp_send_body(c, buf, got) != 0) { rc = -1; break; }
        left -= (int64_t)got;
    }
    free(buf);
    return rc;
}

static unsigned char *read_whole_file(const char *path, int64_t size, size_t *out_len) {
    FILE *f = lith_fopen(path, "rb");
    if (!f) return NULL;
    unsigned char *buf = (unsigned char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if ((int64_t)got != size) { free(buf); return NULL; }
    buf[got] = '\0';
    *out_len = got;
    return buf;
}

static void not_found(LithConn *c, HttpRequest *req) {
    if (g_config.spa_fallback) {
        /* Une route d'application (/profil/42) n'a pas d'extension, un fichier manquant (/logo.png) si. */
        const char *last = strrchr(req->path, '/');
        if (!strchr(last ? last : req->path, '.')) {
            char fs[LITH_PATH_MAX * 2];
            snprintf(fs, sizeof(fs), "%s/%s", g_config.public_dir, g_config.index_file);
            lith_stat_t st;
            if (lith_stat(fs, &st) == 0 && st.is_file) {
                serve_file(c, req, fs, &st);
                return;
            }
        }
    }
    resp_error(c, 404, NULL);
}

/* ---------------------------------------------------------------------- */
/* Liste de fichiers (AUTO_INDEX)                                          */
/* ---------------------------------------------------------------------- */

typedef struct {
    char   *name;
    bool    is_dir;
    int64_t size;
    time_t  mtime;
} DirItem;

static int dir_item_cmp(const void *a, const void *b) {
    const DirItem *x = (const DirItem *)a, *y = (const DirItem *)b;
    if (x->is_dir != y->is_dir) return x->is_dir ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

static void human_size(int64_t size, char *out, size_t n) {
    const char *units[] = { "o", "Ko", "Mo", "Go", "To" };
    double v = (double)size;
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) snprintf(out, n, "%lld %s", (long long)size, units[0]);
    else snprintf(out, n, "%.1f %s", v, units[u]);
}

static void serve_autoindex(LithConn *c, HttpRequest *req, const char *fs_dir) {
    lith_dir_t *d = lith_dir_open(fs_dir);
    if (!d) { resp_error(c, 404, NULL); return; }

    DirItem *items = NULL;
    size_t count = 0, cap = 0;
    const char *name;
    while ((name = lith_dir_next(d)) != NULL && count < AUTOINDEX_MAX_ITEMS) {
        if (g_config.hide_dotfiles && name[0] == '.') continue;
        char full[LITH_PATH_MAX * 2];
        snprintf(full, sizeof(full), "%s/%s", fs_dir, name);
        lith_stat_t st;
        if (lith_stat(full, &st) != 0) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 64;
            DirItem *n = (DirItem *)realloc(items, cap * sizeof(DirItem));
            if (!n) break;
            items = n;
        }
        size_t nl = strlen(name);
        items[count].name = (char *)malloc(nl + 1);
        if (!items[count].name) break;
        memcpy(items[count].name, name, nl + 1);
        items[count].is_dir = st.is_dir;
        items[count].size = st.size;
        items[count].mtime = st.mtime;
        count++;
    }
    lith_dir_close(d);
    if (count) qsort(items, count, sizeof(DirItem), dir_item_cmp);

    char title[HTTP_MAX_PATH * 6];
    http_html_escape(req->path, title, sizeof(title));

    StrBuf sb;
    sb_init(&sb);
    sb_printf(&sb,
        "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "<title>Index de %s</title>\n<style>\n"
        "body{font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:2rem 1rem}\n"
        "main{max-width:900px;margin:0 auto}h1{font-size:1.3rem;word-break:break-all}\n"
        "table{width:100%%;border-collapse:collapse;background:#1e293b;border-radius:8px;overflow:hidden}\n"
        "th,td{text-align:left;padding:.55rem .9rem;border-bottom:1px solid #334155}th{color:#94a3b8;font-weight:600}\n"
        "td.r,th.r{text-align:right;white-space:nowrap}a{color:#34d399;text-decoration:none}a:hover{text-decoration:underline}\n"
        "footer{margin-top:1rem;color:#64748b;font-size:.8rem}\n"
        "</style>\n</head>\n<body>\n<main>\n<h1>Index de %s</h1>\n<table>\n"
        "<tr><th>Nom</th><th class=\"r\">Taille</th><th class=\"r\">Modifié</th></tr>\n",
        title, title);
    if (strcmp(req->path, "/") != 0)
        sb_puts(&sb, "<tr><td><a href=\"../\">../</a></td><td class=\"r\">-</td><td class=\"r\">-</td></tr>\n");

    for (size_t i = 0; i < count; i++) {
        char href[1024], esc[1024], size[32], date[32];
        http_url_encode_path(items[i].name, href, sizeof(href));
        http_html_escape(items[i].name, esc, sizeof(esc));
        if (items[i].is_dir) snprintf(size, sizeof(size), "-");
        else human_size(items[i].size, size, sizeof(size));
        struct tm tm;
        lith_localtime(items[i].mtime, &tm);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &tm);
        sb_printf(&sb, "<tr><td><a href=\"%s%s\">%s%s</a></td><td class=\"r\">%s</td><td class=\"r\">%s</td></tr>\n",
                  href, items[i].is_dir ? "/" : "", esc, items[i].is_dir ? "/" : "", size, date);
        free(items[i].name);
    }
    free(items);
    sb_printf(&sb, "</table>\n<footer>%s</footer>\n</main>\n</body>\n</html>\n", LITH_SERVER_NAME);

    size_t len = 0;
    char *html = sb_take(&sb, &len);
    if (!html) { resp_error(c, 500, NULL); return; }
    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Cache-Control", "no-cache");
    resp_send(c, 200, "text/html; charset=utf-8", html, len, &h);
    free(html);
}

/* ---------------------------------------------------------------------- */
/* Envoi d'un fichier                                                      */
/* ---------------------------------------------------------------------- */

static void serve_html_with_livereload(LithConn *c, const char *fs_path, const lith_stat_t *st, const char *mime) {
    if (st->size > LIVERELOAD_MAX_HTML) { resp_error(c, 500, "HTML file too large for live reload."); return; }
    size_t len = 0;
    unsigned char *html = read_whole_file(fs_path, st->size, &len);
    if (!html) { resp_error(c, 500, NULL); return; }

    const char *script = livereload_script();
    size_t slen = strlen(script);
    /* Insère le script juste avant la dernière balise </body> (ou à la fin). */
    size_t pos = len;
    for (size_t i = len; i >= 7; i--) {
        if (strncasecmp((const char *)html + i - 7, "</body>", 7) == 0) { pos = i - 7; break; }
    }
    unsigned char *out = (unsigned char *)malloc(len + slen);
    if (!out) { free(html); resp_error(c, 500, NULL); return; }
    memcpy(out, html, pos);
    memcpy(out + pos, script, slen);
    memcpy(out + pos + slen, html + pos, len - pos);
    free(html);

    RespHeaders h;
    resp_init(&h);
    resp_add(&h, "Cache-Control", "no-store");
    resp_send(c, 200, mime, out, len + slen, &h);
    free(out);
}

static void serve_file(LithConn *c, HttpRequest *req, const char *fs_path, const lith_stat_t *st) {
    const char *mime = http_mime_type(fs_path);
    char etag[64], lastmod[40];
    make_etag(st, etag, sizeof(etag));
    http_format_date(st->mtime, lastmod, sizeof(lastmod));

    /* 1. Validation conditionnelle : le navigateur a déjà la bonne version. */
    const char *inm = http_header_get(req, "If-None-Match");
    bool not_modified = false;
    if (inm) {
        not_modified = etag_list_matches(inm, etag);
    } else {
        time_t since = http_parse_date(http_header_get(req, "If-Modified-Since"));
        not_modified = (since != (time_t)-1 && st->mtime <= since);
    }
    if (not_modified) {
        RespHeaders h;
        resp_init(&h);
        resp_add(&h, "ETag", "%s", etag);
        resp_add(&h, "Last-Modified", "%s", lastmod);
        if (g_config.cache_control[0]) resp_add(&h, "Cache-Control", "%s", g_config.cache_control);
        resp_send_head(c, 304, &h, -1);
        return;
    }

    /* 2. Rechargement automatique (mode développement). */
    if (g_config.live_reload && strncmp(mime, "text/html", 9) == 0) {
        serve_html_with_livereload(c, fs_path, st, mime);
        return;
    }

    /* 3. Requête partielle "Range: bytes=a-b". */
    const char *range = http_header_get(req, "Range");
    if (range && (strcmp(req->method, "GET") == 0)) {
        const char *if_range = http_header_get(req, "If-Range");
        bool range_ok = !if_range || strcmp(if_range, etag) == 0 || strcmp(if_range, lastmod) == 0;
        if (range_ok) {
            int64_t start = 0, end = 0;
            int r = http_parse_range(range, st->size, &start, &end);
            if (r < 0) {
                RespHeaders h;
                resp_init(&h);
                resp_add(&h, "Content-Range", "bytes */%lld", (long long)st->size);
                resp_error_ex(c, 416, NULL, &h);
                return;
            }
            if (r > 0) {
                FILE *f = lith_fopen(fs_path, "rb");
                if (!f) { resp_error(c, 500, NULL); return; }
                RespHeaders h;
                resp_init(&h);
                add_file_headers(&h, mime, etag, lastmod);
                resp_add(&h, "Accept-Ranges", "bytes");
                resp_add(&h, "Content-Range", "bytes %lld-%lld/%lld", (long long)start, (long long)end, (long long)st->size);
                if (resp_send_head(c, 206, &h, end - start + 1) == 0)
                    stream_file(c, f, start, end - start + 1);
                else
                    c->force_close = true;
                fclose(f);
                return;
            }
        }
    }

    bool want_gzip = g_config.gzip && http_mime_is_compressible(mime) && st->size >= GZIP_MIN_SIZE &&
                     http_accepts_gzip(req);

    RespHeaders h;
    resp_init(&h);
    add_file_headers(&h, mime, etag, lastmod);
    resp_add(&h, "Accept-Ranges", "bytes");

    /* 4a. Cache mémoire. */
    if (g_config.use_cache) {
        bool hit = false;
        CacheEntry *e = cache_acquire(fs_path, st, &hit);
        if (e) {
            metrics_cache(hit);
            const unsigned char *data = e->data;
            size_t len = e->size;
            if (want_gzip && cache_ensure_gzip(e)) {
                data = e->gz;
                len = e->gz_size;
                resp_add(&h, "Content-Encoding", "gzip");
            }
            if (resp_send_head(c, 200, &h, (long long)len) == 0)
                resp_send_body(c, data, len);
            else
                c->force_close = true;
            cache_release(e);
            return;
        }
    }

    /* 4b. Compression à la volée (fichier hors cache mais de taille raisonnable). */
    if (want_gzip && st->size <= GZIP_MAX_ON_THE_FLY) {
        size_t raw_len = 0;
        unsigned char *raw = read_whole_file(fs_path, st->size, &raw_len);
        if (raw) {
            size_t gz_len = 0;
            unsigned char *gz = lith_gzip(raw, raw_len, &gz_len);
            const unsigned char *data = raw;
            size_t len = raw_len;
            if (gz && gz_len < raw_len) {
                data = gz;
                len = gz_len;
                resp_add(&h, "Content-Encoding", "gzip");
            }
            if (resp_send_head(c, 200, &h, (long long)len) == 0)
                resp_send_body(c, data, len);
            else
                c->force_close = true;
            free(gz);
            free(raw);
            return;
        }
    }

    /* 5. Envoi direct depuis le disque. */
    FILE *f = lith_fopen(fs_path, "rb");
    if (!f) { resp_error(c, 403, NULL); return; }
    if (resp_send_head(c, 200, &h, st->size) == 0)
        stream_file(c, f, 0, st->size);
    else
        c->force_close = true;
    fclose(f);
}

/* ---------------------------------------------------------------------- */

void static_serve(LithConn *c, HttpRequest *req) {
    if (g_config.hide_dotfiles && http_path_has_hidden_segment(req->path)) {
        resp_error(c, 404, NULL);
        return;
    }

    char fs[LITH_PATH_MAX + HTTP_MAX_PATH + 8];
    int n = snprintf(fs, sizeof(fs), "%s%s", g_config.public_dir, req->path);
    if (n < 0 || (size_t)n >= sizeof(fs)) { resp_error(c, 414, NULL); return; }

    /* stat() refuse le '/' final sous Windows : on le retire pour l'appel. */
    size_t fl = strlen(fs);
    bool trailing_slash = fl > 0 && fs[fl - 1] == '/';
    while (fl > 1 && fs[fl - 1] == '/') fs[--fl] = '\0';

    lith_stat_t st;
    if (lith_stat(fs, &st) != 0) { not_found(c, req); return; }

    if (st.is_dir) {
        if (!trailing_slash) {
            /* /docs -> /docs/ pour que les liens relatifs de la page fonctionnent */
            RespHeaders h;
            resp_init(&h);
            if (req->query[0]) resp_add(&h, "Location", "%s/?%s", req->raw_path, req->query);
            else resp_add(&h, "Location", "%s/", req->raw_path);
            resp_send(c, 301, "text/plain; charset=utf-8", "Moved Permanently\n", 18, &h);
            return;
        }
        char idx[sizeof(fs) + 160];
        snprintf(idx, sizeof(idx), "%s/%s", fs, g_config.index_file);
        lith_stat_t ist;
        if (lith_stat(idx, &ist) == 0 && ist.is_file) {
            serve_file(c, req, idx, &ist);
            return;
        }
        if (g_config.auto_index) {
            serve_autoindex(c, req, fs);
            return;
        }
        not_found(c, req);
        return;
    }

    if (!st.is_file || trailing_slash) { not_found(c, req); return; }
    serve_file(c, req, fs, &st);
}
