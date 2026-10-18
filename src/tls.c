/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * tls.c — E/S des connexions clientes : socket brute, ou HTTPS via le module
 * Rust lith_tls (tls/, rustls) quand LITH est compilé avec `make TLS=1`.
 *
 * Le module TLS ne voit jamais la socket : on lui passe les octets reçus et on
 * envoie nous-mêmes ce qu'il produit. Les délais (SO_RCVTIMEO), le keep-alive
 * et la protection anti-slowloris du serveur restent donc inchangés.
 */

#include "tls.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>

#ifdef LITH_TLS
#include "lith_tls.h"

#define TLS_IO_CHUNK  16384   /* un enregistrement TLS au plus : le texte clair tient dans rustls */
#define TLS_SLICE_MS  500     /* rend la main à l'appelant pour qu'il vérifie ses délais */

static RtlsConfig *g_tls_cfg;

bool tls_compiled(void) { return true; }

bool tls_check_files(const char *cert, const char *key, char *err, size_t err_size) {
    RtlsConfig *cfg = rtls_config_new(cert, key, err, err_size);
    if (!cfg) return false;
    rtls_config_free(cfg);
    return true;
}

int tls_server_init(const char *cert, const char *key) {
    char err[512] = "";
    g_tls_cfg = rtls_config_new(cert, key, err, sizeof(err));
    if (!g_tls_cfg) {
        lith_log(LOG_ERROR, "HTTPS : %s", err);
        return -1;
    }
    return 0;
}

void tls_server_cleanup(void) {
    rtls_config_free(g_tls_cfg);
    g_tls_cfg = NULL;
}

int lith_stream_start_tls(LithStream *s) {
    if (!g_tls_cfg) return -1;
    s->tls = rtls_accept(g_tls_cfg);
    return s->tls ? 0 : -1;
}

/* Envoie tout ce que le moteur TLS a produit (poignée de main, données, alertes). */
static int tls_flush(LithStream *s) {
    unsigned char out[TLS_IO_CHUNK];
    for (;;) {
        long n = rtls_take_output(s->tls, out, sizeof(out));
        if (n < 0) return -1;
        if (n == 0) return 0;
        if (lith_send_all(s->sock, out, (size_t)n) != 0) return -1;
    }
}

static void tls_log_error(const LithStream *s, const char *when) {
    char err[256];
    rtls_last_error(s->tls, err, sizeof(err));
    lith_log(LOG_DEBUG, "HTTPS : %s (%s)", when, err[0] ? err : "connexion interrompue");
}

static long tls_recv(LithStream *s, void *buf, size_t len) {
    uint64_t start = lith_now_ms();
    unsigned char in[TLS_IO_CHUNK];
    for (;;) {
        long n = rtls_read(s->tls, (unsigned char *)buf, len);
        if (n > 0) return n;
        if (n == RTLS_EOF) return 0;
        if (n < 0) {
            tls_log_error(s, "lecture impossible");
            lith_set_last_error(false);
            return -1;
        }
        /* Un client qui distille sa poignée de main octet par octet ne doit pas
         * échapper aux délais du serveur : on rend régulièrement la main. */
        if (lith_now_ms() - start >= TLS_SLICE_MS) {
            lith_set_last_error(true);
            return -1;
        }

        long r = lith_recv(s->sock, in, sizeof(in));
        if (r <= 0) return r;   /* fermée, erreur ou délai : l'erreur système est intacte */
        int fe = rtls_feed(s->tls, in, (size_t)r);
        int fl = tls_flush(s);  /* réponse de poignée de main ou alerte */
        if (fe != 0 || fl != 0) {
            if (fe != 0) tls_log_error(s, rtls_is_handshaking(s->tls) ? "poignée de main refusée" : "données invalides");
            lith_set_last_error(false);
            return -1;
        }
    }
}

static int tls_send_all(LithStream *s, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    int stalled = 0;
    while (len > 0) {
        long n = rtls_write(s->tls, p, len);
        if (n < 0) return -1;
        if (tls_flush(s) != 0) return -1;
        if (n == 0 && ++stalled > 2) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

#else /* ---- compilation sans TLS : connexions en clair uniquement ---- */

bool tls_compiled(void) { return false; }

bool tls_check_files(const char *cert, const char *key, char *err, size_t err_size) {
    (void)cert;
    (void)key;
    snprintf(err, err_size, "LITH a été compilé sans HTTPS (recompilez avec : make TLS=1)");
    return false;
}

int  tls_server_init(const char *cert, const char *key) {
    (void)cert;
    (void)key;
    lith_log(LOG_ERROR, "HTTPS : LITH a été compilé sans TLS (recompilez avec : make TLS=1)");
    return -1;
}

void tls_server_cleanup(void) {}

int  lith_stream_start_tls(LithStream *s) {
    (void)s;
    return -1;
}

#endif /* LITH_TLS */

long lith_stream_recv(LithStream *s, void *buf, size_t len) {
#ifdef LITH_TLS
    if (s->tls) return tls_recv(s, buf, len);
#endif
    return lith_recv(s->sock, buf, len);
}

int lith_stream_send_all(LithStream *s, const void *buf, size_t len) {
#ifdef LITH_TLS
    if (s->tls) return tls_send_all(s, buf, len);
#endif
    return lith_send_all(s->sock, buf, len);
}

void lith_stream_close(LithStream *s, bool linger) {
#ifdef LITH_TLS
    if (s->tls && !rtls_is_handshaking(s->tls)) {
        rtls_close(s->tls);
        tls_flush(s);
    }
#endif
    if (linger) {
        /* Fermeture "douce" : laisse le client lire la réponse avant le reset TCP. */
#ifdef _WIN32
        shutdown(s->sock, SD_SEND);
#else
        shutdown(s->sock, SHUT_WR);
#endif
        char tmp[4096];
        for (int i = 0; i < 4; i++)
            if (lith_recv(s->sock, tmp, sizeof(tmp)) <= 0) break;
    }
    lith_socket_close(s->sock);
    s->sock = LITH_INVALID_SOCKET;
#ifdef LITH_TLS
    rtls_free(s->tls);
#endif
    s->tls = NULL;
}
