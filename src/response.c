/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "server.h"
#include "config.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void resp_init(RespHeaders *h) {
    h->len = 0;
    h->buf[0] = '\0';
}

void resp_add(RespHeaders *h, const char *name, const char *fmt, ...) {
    char value[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);
    /* Un en-tête ne doit jamais contenir de retour à la ligne (injection d'en-têtes). */
    for (char *p = value; *p; p++)
        if (*p == '\r' || *p == '\n') *p = ' ';
    int n = snprintf(h->buf + h->len, sizeof(h->buf) - h->len, "%s: %s\r\n", name, value);
    if (n > 0 && h->len + (size_t)n < sizeof(h->buf)) h->len += (size_t)n;
    else h->buf[h->len] = '\0';
}

static bool status_has_body(int status) {
    return !(status < 200 || status == 204 || status == 304);
}

int resp_send_head(LithConn *c, int status, const RespHeaders *extra, long long content_length) {
    if (c->headers_sent) return -1;
    c->status = status;
    c->headers_sent = true;

    bool ka = c->req && c->req->keep_alive && !c->force_close && !server_stopping() &&
              g_config.keepalive_timeout > 0 && c->requests_served + 1 < g_config.keepalive_max;
    if (ka && status_has_body(status) && content_length < 0) ka = false;
    /* Sous forte charge, on libère le thread pour les clients en attente. */
    if (ka && server_is_busy()) ka = false;
    c->keep_alive = ka;

    char head[8192];
    size_t len = 0;
    char date[40];
    http_format_date(time(NULL), date, sizeof(date));

#define APPEND(...) do { \
        int _n = snprintf(head + len, sizeof(head) - len, __VA_ARGS__); \
        if (_n > 0 && len + (size_t)_n < sizeof(head)) len += (size_t)_n; \
    } while (0)

    APPEND("HTTP/1.1 %d %s\r\n", status, http_status_text(status));
    APPEND("Server: %s\r\nDate: %s\r\n", LITH_SERVER_NAME, date);
    if (content_length >= 0 && status_has_body(status))
        APPEND("Content-Length: %lld\r\n", content_length);
    if (extra && extra->len) APPEND("%s", extra->buf);

    if (g_config.security_headers && status != 101) {
        APPEND("X-Content-Type-Options: nosniff\r\n");
        APPEND("X-Frame-Options: SAMEORIGIN\r\n");
        APPEND("Referrer-Policy: strict-origin-when-cross-origin\r\n");
        if (g_config.csp[0]) APPEND("Content-Security-Policy: %s\r\n", g_config.csp);
    }
    /* HSTS n'a de sens (et n'est pris en compte par les navigateurs) qu'en HTTPS. */
    if (g_config.hsts > 0 && conn_is_https(c) && status != 101)
        APPEND("Strict-Transport-Security: max-age=%d\r\n", g_config.hsts);
    if (g_config.cors_origin[0] && status != 101) {
        APPEND("Access-Control-Allow-Origin: %s\r\n", g_config.cors_origin);
        if (strcmp(g_config.cors_origin, "*") != 0) APPEND("Vary: Origin\r\n");
    }
    if (status != 101) {
        if (ka) APPEND("Connection: keep-alive\r\nKeep-Alive: timeout=%d, max=%d\r\n",
                       g_config.keepalive_timeout, g_config.keepalive_max - c->requests_served - 1);
        else APPEND("Connection: close\r\n");
    }
    APPEND("\r\n");
#undef APPEND

    return lith_stream_send_all(&c->io, head, len);
}

int resp_send_body(LithConn *c, const void *data, size_t len) {
    if (c->req && c->req->is_head) return 0;
    if (len == 0) return 0;
    if (lith_stream_send_all(&c->io, data, len) != 0) {
        c->force_close = true;
        c->keep_alive = false;
        return -1;
    }
    c->bytes_sent += (long long)len;
    return 0;
}

int resp_send(LithConn *c, int status, const char *content_type, const void *body, size_t len, const RespHeaders *extra) {
    RespHeaders h;
    resp_init(&h);
    if (content_type) resp_add(&h, "Content-Type", "%s", content_type);
    if (extra && extra->len && h.len + extra->len < sizeof(h.buf)) {
        memcpy(h.buf + h.len, extra->buf, extra->len + 1);
        h.len += extra->len;
    }
    if (resp_send_head(c, status, &h, (long long)len) != 0) {
        c->force_close = true;
        return -1;
    }
    return resp_send_body(c, body, len);
}

static const char *error_description(int status) {
    switch (status) {
        case 400: return "The server could not understand this request.";
        case 401: return "Authentication is required to access this resource.";
        case 403: return "Access to this resource is forbidden.";
        case 404: return "The requested resource could not be found on this server.";
        case 405: return "This method is not allowed for the requested resource.";
        case 408: return "The server timed out waiting for the request.";
        case 413: return "The request body is larger than the server is willing to accept.";
        case 414: return "The requested URL is too long.";
        case 416: return "The requested range cannot be satisfied.";
        case 429: return "Too many requests. Please slow down and try again shortly.";
        case 431: return "The request headers are too large.";
        case 500: return "The server encountered an unexpected condition.";
        case 501: return "The server does not support this feature.";
        case 502: return "The upstream server is unreachable or sent an invalid response.";
        case 503: return "The server is temporarily unable to handle the request.";
        case 504: return "The upstream server did not respond in time.";
        case 505: return "This HTTP version is not supported.";
        default:  return "An error occurred.";
    }
}

/* Page d'erreur personnalisée : public/<code>.html si elle existe. */
static char *load_custom_page(int status, size_t *len) {
    char path[LITH_PATH_MAX + 16];
    snprintf(path, sizeof(path), "%s/%d.html", g_config.public_dir, status);
    lith_stat_t st;
    if (lith_stat(path, &st) != 0 || !st.is_file || st.size > 256 * 1024) return NULL;
    FILE *f = lith_fopen(path, "rb");
    if (!f) return NULL;
    char *buf = (char *)malloc((size_t)st.size + 1);
    if (!buf) { fclose(f); return NULL; }
    *len = fread(buf, 1, (size_t)st.size, f);
    fclose(f);
    buf[*len] = '\0';
    return buf;
}

void resp_error_ex(LithConn *c, int status, const char *detail, const RespHeaders *extra) {
    if (c->headers_sent) {
        c->force_close = true;
        return;
    }
    if (status == 400 || status == 408 || status == 413 || status == 414 || status == 431 || status >= 500)
        c->force_close = true;

    size_t len = 0;
    char *custom = (status >= 400) ? load_custom_page(status, &len) : NULL;
    if (custom) {
        resp_send(c, status, "text/html; charset=utf-8", custom, len, extra);
        free(custom);
        return;
    }

    char escaped[512];
    http_html_escape(detail ? detail : error_description(status), escaped, sizeof(escaped));
    char html[4096];
    int n = snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "<title>%d %s</title>\n<style>\n"
        "body{font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:#0f172a;color:#f8fafc;"
        "display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}\n"
        ".card{text-align:center;background:#1e293b;padding:3rem 2rem;border-radius:12px;"
        "box-shadow:0 10px 25px rgba(0,0,0,.3);max-width:460px;width:90%%;border:1px solid rgba(239,68,68,.2)}\n"
        "h1{font-size:4rem;margin:0;color:#ef4444;font-weight:800}\n"
        "h2{font-size:1.4rem;margin:.5rem 0 1rem;color:#e2e8f0}\n"
        "p{color:#94a3b8;line-height:1.6;margin-bottom:2rem}\n"
        ".footer{font-size:.8rem;color:#64748b;border-top:1px solid #334155;padding-top:1rem}\n"
        "</style>\n</head>\n<body>\n<div class=\"card\">\n<h1>%d</h1>\n<h2>%s</h2>\n<p>%s</p>\n"
        "<div class=\"footer\">%s</div>\n</div>\n</body>\n</html>\n",
        status, http_status_text(status), status, http_status_text(status), escaped, LITH_SERVER_NAME);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(html)) n = (int)sizeof(html) - 1;
    resp_send(c, status, "text/html; charset=utf-8", html, (size_t)n, extra);
}

void resp_error(LithConn *c, int status, const char *detail) {
    resp_error_ex(c, status, detail, NULL);
}
