/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * lith_tls.h — API C du module Rust lith_tls (rustls), utilisé avec `make TLS=1`.
 *
 * Le module ne touche jamais aux sockets : LITH lui transmet les octets reçus
 * (rtls_feed), récupère le texte clair (rtls_read), chiffre ses réponses
 * (rtls_write) puis envoie lui-même les octets produits (rtls_take_output).
 * Une session n'est jamais utilisée par deux threads en même temps.
 */

#ifndef LITH_RTLS_H
#define LITH_RTLS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTLS_EOF   (-1)   /* le client a fermé proprement (close_notify) */
#define RTLS_ERROR (-2)   /* erreur fatale : abandonner la connexion */

typedef struct RtlsConfig  RtlsConfig;
typedef struct RtlsSession RtlsSession;

/* Charge le certificat (chaîne PEM) et la clé privée (PEM). NULL si échec, message dans err. */
RtlsConfig  *rtls_config_new(const char *cert_path, const char *key_path, char *err, size_t err_size);
void         rtls_config_free(RtlsConfig *cfg);

/* Nouvelle connexion serveur ; la poignée de main avance au fil de rtls_feed. */
RtlsSession *rtls_accept(const RtlsConfig *cfg);
void         rtls_free(RtlsSession *s);

/* Octets reçus du réseau. 0 = ok, RTLS_ERROR = abandonner (après avoir envoyé la sortie en attente). */
int          rtls_feed(RtlsSession *s, const unsigned char *data, size_t len);
/* Texte clair : > 0 octets, 0 = recevoir d'abord, RTLS_EOF, RTLS_ERROR. */
long         rtls_read(RtlsSession *s, unsigned char *buf, size_t len);
/* Chiffre du texte clair : octets acceptés (peut être < len), ou RTLS_ERROR. */
long         rtls_write(RtlsSession *s, const unsigned char *buf, size_t len);
/* Octets chiffrés à envoyer : copiés dans buf, 0 = rien en attente. */
long         rtls_take_output(RtlsSession *s, unsigned char *buf, size_t len);

int          rtls_is_handshaking(const RtlsSession *s);
void         rtls_close(RtlsSession *s);   /* prépare l'alerte close_notify */
void         rtls_last_error(const RtlsSession *s, char *err, size_t err_size);

#ifdef __cplusplus
}
#endif

#endif /* LITH_RTLS_H */
