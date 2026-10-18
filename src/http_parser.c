/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "http.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Caractères autorisés dans un nom de méthode ou d'en-tête (RFC 9110 "tchar"). */
static bool is_tchar(unsigned char c) {
    if (isalnum(c)) return true;
    return strchr("!#$%&'*+-.^_`|~", c) != NULL && c != '\0';
}

static char *trim_ows(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
    return s;
}

int http_parse_request(char *buf, size_t header_len, HttpRequest *req) {
    char *body = req->body;
    size_t body_len = req->body_len;
    memset(req, 0, sizeof(*req));
    req->body = body;
    req->body_len = body_len;
    req->content_length = -1;
    req->query = "";

    if (header_len == 0) return 400;
    if (memchr(buf, '\0', header_len)) return 400;
    buf[header_len] = '\0';
    char *end = buf + header_len;

    /* ---- Ligne de requête : MÉTHODE SP CIBLE SP HTTP/1.x ---- */
    char *line_end = strstr(buf, "\r\n");
    if (!line_end) line_end = end;
    *line_end = '\0';

    char *sp1 = strchr(buf, ' ');
    if (!sp1 || sp1 == buf) return 400;
    size_t mlen = (size_t)(sp1 - buf);
    for (size_t i = 0; i < mlen; i++)
        if (!is_tchar((unsigned char)buf[i])) return 400;
    if (mlen >= sizeof(req->method)) return 501;
    memcpy(req->method, buf, mlen);
    req->method[mlen] = '\0';

    char *target = sp1 + 1;
    char *sp2 = strchr(target, ' ');
    if (!sp2 || sp2 == target) return 400;
    *sp2 = '\0';
    const char *version = sp2 + 1;

    if (strncmp(version, "HTTP/", 5) != 0) return 400;
    if (version[5] != '1' || version[6] != '.' || !isdigit((unsigned char)version[7]) || version[8] != '\0')
        return 505;
    req->version_minor = (version[7] == '0') ? 0 : 1;

    for (const char *t = target; *t; t++) {
        unsigned char c = (unsigned char)*t;
        if (c <= 0x20 || c == 0x7f) return 400;
    }
    if (strlen(target) >= HTTP_MAX_PATH) return 414;
    req->target = target;

    /* Forme absolue "http://hote/chemin" tolérée : on ne garde que le chemin. */
    const char *path_start = target;
    if (strncasecmp(path_start, "http://", 7) == 0) {
        path_start = strchr(path_start + 7, '/');
        if (!path_start) path_start = "/";
    }
    if (strcmp(path_start, "*") == 0) {
        if (strcmp(req->method, "OPTIONS") != 0) return 400;
        snprintf(req->raw_path, sizeof(req->raw_path), "*");
        snprintf(req->path, sizeof(req->path), "*");
    } else {
        if (path_start[0] != '/') return 400;
        size_t plen = strcspn(path_start, "?#");
        if (plen >= sizeof(req->raw_path)) return 414;
        memcpy(req->raw_path, path_start, plen);
        req->raw_path[plen] = '\0';
        if (path_start[plen] == '?') {
            req->query = path_start + plen + 1;
        }
        if (http_url_decode(req->raw_path, plen, req->path, sizeof(req->path)) < 0) return 400;
    }

    /* ---- En-têtes ---- */
    char *p = (line_end < end) ? line_end + 2 : end;
    while (p < end) {
        char *eol = strstr(p, "\r\n");
        if (!eol) eol = end;
        *eol = '\0';

        if (*p == ' ' || *p == '\t') return 400;          /* repli de ligne obsolète */
        char *colon = strchr(p, ':');
        if (!colon || colon == p) return 400;
        for (char *n = p; n < colon; n++)
            if (!is_tchar((unsigned char)*n)) return 400;  /* espace avant ':' interdit */
        *colon = '\0';
        if (req->header_count >= HTTP_MAX_HEADERS) return 431;
        req->headers[req->header_count].name = p;
        req->headers[req->header_count].value = trim_ows(colon + 1);
        req->header_count++;

        p = (eol < end) ? eol + 2 : end;
    }

    /* ---- Sémantique ---- */
    bool has_host = false;
    const char *te = NULL;
    for (int i = 0; i < req->header_count; i++) {
        const char *name = req->headers[i].name;
        const char *value = req->headers[i].value;
        if (!strcasecmp(name, "Host")) {
            if (has_host) return 400;
            has_host = true;
        } else if (!strcasecmp(name, "Content-Length")) {
            if (!*value) return 400;
            for (const char *v = value; *v; v++)
                if (!isdigit((unsigned char)*v)) return 400;
            if (strlen(value) > 15) return 400;
            long long cl = strtoll(value, NULL, 10);
            if (req->content_length >= 0 && req->content_length != cl) return 400;
            req->content_length = cl;
        } else if (!strcasecmp(name, "Transfer-Encoding")) {
            te = value;
        }
    }
    if (req->version_minor == 1 && !has_host) return 400;
    if (te) {
        /* Content-Length + Transfer-Encoding ensemble = tentative de "request smuggling" */
        if (req->content_length >= 0) return 400;
        req->chunked = http_header_has_token(te, "chunked");
        if (!req->chunked) return 501;
    }

    const char *conn = http_header_get(req, "Connection");
    if (req->version_minor == 1)
        req->keep_alive = !(conn && http_header_has_token(conn, "close"));
    else
        req->keep_alive = conn && http_header_has_token(conn, "keep-alive");

    req->is_head = strcmp(req->method, "HEAD") == 0;
    return 0;
}

const char *http_header_get(const HttpRequest *req, const char *name) {
    for (int i = 0; i < req->header_count; i++)
        if (!strcasecmp(req->headers[i].name, name)) return req->headers[i].value;
    return NULL;
}

/* Cherche un jeton dans une liste séparée par des virgules ("keep-alive, Upgrade"). */
bool http_header_has_token(const char *value, const char *token) {
    if (!value || !token) return false;
    size_t tlen = strlen(token);
    const char *p = value;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        const char *start = p;
        while (*p && *p != ',' && *p != ';') p++;
        const char *stop = p;
        while (stop > start && (stop[-1] == ' ' || stop[-1] == '\t')) stop--;
        if ((size_t)(stop - start) == tlen && strncasecmp(start, token, tlen) == 0) return true;
        while (*p && *p != ',') p++;
    }
    return false;
}

bool http_accepts_gzip(const HttpRequest *req) {
    const char *ae = http_header_get(req, "Accept-Encoding");
    if (!ae) return false;
    /* Refuse explicitement "gzip;q=0" */
    const char *g = NULL;
    for (const char *s = ae; *s; s++)
        if (strncasecmp(s, "gzip", 4) == 0) { g = s; break; }
    if (!g) return false;
    const char *q = g + 4;
    while (*q == ' ') q++;
    if (*q == ';') {
        q++;
        while (*q == ' ') q++;
        if ((q[0] == 'q' || q[0] == 'Q') && q[1] == '=') {
            double v = atof(q + 2);
            if (v <= 0.0) return false;
        }
    }
    return true;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int http_url_decode(const char *src, size_t len, char *dst, size_t dst_size) {
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if (c == '%') {
            if (i + 2 >= len) return -1;
            int h = hexval(src[i + 1]);
            int l = hexval(src[i + 2]);
            if (h < 0 || l < 0) return -1;
            c = (char)(h * 16 + l);
            if (c == '\0') return -1;
            i += 2;
        }
        if (o + 1 >= dst_size) return -1;
        dst[o++] = c;
    }
    dst[o] = '\0';
    return (int)o;
}

void http_url_encode_path(const char *src, char *dst, size_t dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && o + 4 < dst_size; p++) {
        if (isalnum(*p) || strchr("-._~/!$&'()*+,;=:@", *p)) {
            dst[o++] = (char)*p;
        } else {
            dst[o++] = '%';
            dst[o++] = hex[*p >> 4];
            dst[o++] = hex[*p & 15];
        }
    }
    dst[o] = '\0';
}

void http_html_escape(const char *src, char *dst, size_t dst_size) {
    size_t o = 0;
    for (const char *p = src; *p; p++) {
        const char *rep = NULL;
        switch (*p) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: break;
        }
        if (rep) {
            size_t rl = strlen(rep);
            if (o + rl + 1 >= dst_size) break;
            memcpy(dst + o, rep, rl);
            o += rl;
        } else {
            if (o + 2 >= dst_size) break;
            dst[o++] = *p;
        }
    }
    dst[o] = '\0';
}

bool http_path_is_safe(const char *path) {
    if (!path || path[0] != '/') return false;
    if (strchr(path, '\\')) return false;
#ifdef _WIN32
    if (strchr(path, ':')) return false;   /* lecteurs "C:" et flux NTFS "fichier::$DATA" */
#endif
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - seg);
        if (n == 2 && seg[0] == '.' && seg[1] == '.') return false;
#ifdef _WIN32
        /* Windows ignore les points et espaces finaux : ".. " équivaudrait à ".." */
        if (n > 0 && (seg[n - 1] == '.' || seg[n - 1] == ' ') && !(n == 1 && seg[0] == '.')) return false;
#endif
    }
    return true;
}

bool http_path_has_hidden_segment(const char *path) {
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '.') {
            const char *seg = p;
            while (*p && *p != '/') p++;
            size_t n = (size_t)(p - seg);
            if (n == 1) continue;                                   /* "." */
            if (n == 11 && strncmp(seg, ".well-known", 11) == 0) continue;
            return true;
        }
        while (*p && *p != '/') p++;
    }
    return false;
}

const char *http_status_text(int code) {
    switch (code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Content Too Large";
        case 414: return "URI Too Long";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

typedef struct { const char *ext; const char *mime; } MimeEntry;

static const MimeEntry MIME_TABLE[] = {
    { "html", "text/html; charset=utf-8" },
    { "htm",  "text/html; charset=utf-8" },
    { "css",  "text/css; charset=utf-8" },
    { "js",   "text/javascript; charset=utf-8" },
    { "mjs",  "text/javascript; charset=utf-8" },
    { "json", "application/json" },
    { "map",  "application/json" },
    { "webmanifest", "application/manifest+json" },
    { "xml",  "application/xml" },
    { "rss",  "application/rss+xml" },
    { "atom", "application/atom+xml" },
    { "txt",  "text/plain; charset=utf-8" },
    { "md",   "text/markdown; charset=utf-8" },
    { "csv",  "text/csv; charset=utf-8" },
    { "yaml", "text/yaml; charset=utf-8" },
    { "yml",  "text/yaml; charset=utf-8" },
    { "svg",  "image/svg+xml" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "gif",  "image/gif" },
    { "webp", "image/webp" },
    { "avif", "image/avif" },
    { "ico",  "image/x-icon" },
    { "bmp",  "image/bmp" },
    { "woff", "font/woff" },
    { "woff2","font/woff2" },
    { "ttf",  "font/ttf" },
    { "otf",  "font/otf" },
    { "mp4",  "video/mp4" },
    { "webm", "video/webm" },
    { "ogv",  "video/ogg" },
    { "mp3",  "audio/mpeg" },
    { "ogg",  "audio/ogg" },
    { "wav",  "audio/wav" },
    { "flac", "audio/flac" },
    { "m4a",  "audio/mp4" },
    { "pdf",  "application/pdf" },
    { "zip",  "application/zip" },
    { "gz",   "application/gzip" },
    { "tar",  "application/x-tar" },
    { "wasm", "application/wasm" },
};

const char *http_mime_type(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot || !dot[1]) return "application/octet-stream";
    for (size_t i = 0; i < sizeof(MIME_TABLE) / sizeof(MIME_TABLE[0]); i++)
        if (!strcasecmp(dot + 1, MIME_TABLE[i].ext)) return MIME_TABLE[i].mime;
    return "application/octet-stream";
}

bool http_mime_is_compressible(const char *mime) {
    if (!strncmp(mime, "text/", 5)) return true;
    return !strncmp(mime, "application/json", 16) || !strncmp(mime, "application/xml", 15) ||
           !strncmp(mime, "application/manifest+json", 25) || !strncmp(mime, "application/rss+xml", 19) ||
           !strncmp(mime, "application/atom+xml", 20) || !strncmp(mime, "image/svg+xml", 13) ||
           !strncmp(mime, "application/wasm", 16) || !strncmp(mime, "font/ttf", 8) || !strncmp(mime, "font/otf", 8);
}

bool http_method_is_known(const char *m) {
    static const char *known[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "TRACE", "CONNECT" };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (!strcmp(m, known[i])) return true;
    return false;
}

static const char *DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

void http_format_date(time_t t, char *out, size_t size) {
    struct tm tm;
    lith_gmtime(t, &tm);
    snprintf(out, size, "%s, %02d %s %04d %02d:%02d:%02d GMT",
             DAYS[tm.tm_wday % 7], tm.tm_mday, MONTHS[tm.tm_mon % 12], tm.tm_year + 1900,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

time_t http_parse_date(const char *s) {
    if (!s) return (time_t)-1;
    char wday[4], mon[4];
    int day, year, h, m, sec;
    if (sscanf(s, "%3s, %d %3s %d %d:%d:%d GMT", wday, &day, mon, &year, &h, &m, &sec) != 7)
        return (time_t)-1;
    int month = -1;
    for (int i = 0; i < 12; i++)
        if (!strcmp(mon, MONTHS[i])) { month = i; break; }
    if (month < 0 || day < 1 || day > 31 || year < 1970 || h > 23 || m > 59 || sec > 60) return (time_t)-1;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = sec;
    return lith_timegm(&tm);
}

static bool parse_i64(const char *s, size_t n, int64_t *out) {
    if (n == 0 || n > 18) return false;
    int64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return true;
}

int http_parse_range(const char *value, int64_t size, int64_t *start, int64_t *end) {
    if (!value || strncasecmp(value, "bytes=", 6) != 0) return 0;
    const char *r = value + 6;
    while (*r == ' ') r++;
    if (strchr(r, ',')) return 0;                 /* plusieurs plages : on sert le fichier entier */
    const char *dash = strchr(r, '-');
    if (!dash) return 0;
    size_t alen = (size_t)(dash - r);
    const char *b = dash + 1;
    size_t blen = strlen(b);
    while (blen > 0 && b[blen - 1] == ' ') blen--;

    int64_t a = 0, z = 0;
    if (alen == 0) {
        /* "-N" : les N derniers octets */
        if (!parse_i64(b, blen, &z)) return 0;
        if (z == 0 || size == 0) return -1;
        if (z > size) z = size;
        *start = size - z;
        *end = size - 1;
        return 1;
    }
    if (!parse_i64(r, alen, &a)) return 0;
    if (blen == 0) {
        z = size - 1;
    } else {
        if (!parse_i64(b, blen, &z)) return 0;
        if (z < a) return 0;
    }
    if (a >= size) return -1;
    if (z >= size) z = size - 1;
    *start = a;
    *end = z;
    return 1;
}
