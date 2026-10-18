/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * tls.h — E/S d'une connexion cliente, en clair (HTTP) ou chiffrée (HTTPS).
 *
 * Tout le code qui parle à un client passe par LithStream : quand la
 * connexion est chiffrée, recv/send traversent le module TLS (rustls,
 * compilé avec `make TLS=1`), sinon ils vont directement à la socket.
 * Sans TLS=1, ce fichier n'ajoute aucune dépendance : tls est toujours NULL.
 */

#ifndef LITH_TLS_H
#define LITH_TLS_H

#include <stdbool.h>
#include <stddef.h>
#include "platform.h"

typedef struct RtlsSession RtlsSession;

typedef struct {
    socket_t     sock;
    RtlsSession *tls;            /* NULL : connexion en clair */
} LithStream;

/* Vrai si LITH a été compilé avec la prise en charge de HTTPS (make TLS=1). */
bool tls_compiled(void);
/* Vérifie qu'un certificat et une clé PEM sont utilisables (lith check, démarrage). */
bool tls_check_files(const char *cert, const char *key, char *err, size_t err_size);
/* Charge le contexte serveur partagé. 0 si ok. */
int  tls_server_init(const char *cert, const char *key);
void tls_server_cleanup(void);

/* Démarre le chiffrement sur une connexion acceptée. 0 si ok. */
int  lith_stream_start_tls(LithStream *s);
/* Comme lith_recv : > 0 octets, 0 = fermée, -1 = erreur (lith_last_error_is_timeout() valable). */
long lith_stream_recv(LithStream *s, void *buf, size_t len);
/* Comme lith_send_all : 0 si tout est parti. */
int  lith_stream_send_all(LithStream *s, const void *buf, size_t len);
/* Ferme la connexion (close_notify TLS) ; linger : laisse le client lire la réponse avant le reset. */
void lith_stream_close(LithStream *s, bool linger);

static inline bool lith_stream_is_tls(const LithStream *s) { return s->tls != NULL; }

#endif /* LITH_TLS_H */
