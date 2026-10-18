/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

/*
 * config.h — Configuration du serveur (fichier lith.conf + options CLI).
 */

#ifndef LITH_CONFIG_H
#define LITH_CONFIG_H

#include <stdbool.h>
#include "logger.h"
#include "platform.h"

#define LITH_VERSION      "1.0.0"
#define LITH_SERVER_NAME  "LITH/" LITH_VERSION

#define MAX_PROXY_ROUTES    16
#define MAX_PROXY_BACKENDS  8
#define MAX_AUTH_RULES      16

typedef struct {
    char host[256];
    char port[8];
} BackendConf;

typedef struct {
    char prefix[256];                 /* ex: "/api" */
    BackendConf backends[MAX_PROXY_BACKENDS];
    int backend_count;
} ProxyRouteConf;

typedef struct {
    char prefix[256];                 /* ex: "/admin" */
    char credentials[256];            /* "utilisateur:motdepasse" */
} AuthRuleConf;

typedef struct {
    /* --- Réseau --- */
    char      host[64];
    int       port;
    int       threads;               /* 0 = automatique */
    int       max_pending;           /* connexions en attente dans la file */
    int       keepalive_timeout;     /* secondes */
    int       keepalive_max;         /* requêtes max par connexion */
    int       request_timeout;       /* secondes pour recevoir une requête complète */
    long long max_body_size;         /* octets */

    /* --- HTTPS (make TLS=1) --- */
    int  tls_port;                   /* 0 = HTTPS désactivé */
    char tls_cert[LITH_PATH_MAX];    /* chaîne de certificats PEM */
    char tls_key[LITH_PATH_MAX];     /* clé privée PEM */
    bool tls_redirect;               /* le port HTTP redirige vers HTTPS */
    int  hsts;                       /* max-age de Strict-Transport-Security (0 = non envoyé) */

    /* --- Fichiers --- */
    char public_dir[LITH_PATH_MAX];
    char index_file[128];
    bool auto_index;
    bool spa_fallback;
    bool hide_dotfiles;
    char cache_control[128];

    /* --- Performance --- */
    bool      use_cache;
    long long cache_max_size;
    long long cache_file_max;
    bool      gzip;

    /* --- Sécurité --- */
    bool security_headers;
    char csp[512];
    char cors_origin[256];
    int  rate_limit;                 /* requêtes / seconde / IP (0 = désactivé) */
    int  rate_burst;
    AuthRuleConf auth[MAX_AUTH_RULES];
    int  auth_count;

    /* --- Proxy inverse --- */
    ProxyRouteConf proxies[MAX_PROXY_ROUTES];
    int  proxy_count;
    int  proxy_timeout;              /* secondes */
    int  health_interval;            /* secondes (0 = désactivé) */

    /* --- Développement & supervision --- */
    bool live_reload;
    bool status_enabled;
    char status_auth[256];

    /* --- Journaux & processus --- */
    LogLevel log_level;
    char log_file[LITH_PATH_MAX];
    bool access_log;
    char pid_file[LITH_PATH_MAX];
} LithConfig;

/* Configuration globale, en lecture seule une fois le serveur démarré. */
extern LithConfig g_config;

void config_set_defaults(LithConfig *cfg);

/* Retourne 0 si chargé, 1 si le fichier n'existe pas, -1 en cas d'erreurs de syntaxe. */
int  config_load(const char *path, LithConfig *cfg);

/* Applique une paire CLÉ=VALEUR. Retourne false (et écrit un message dans err) si invalide. */
bool config_apply(LithConfig *cfg, const char *key, const char *value, char *err, size_t err_size);

/* Vérifie la cohérence finale. Retourne le nombre d'erreurs (affichées dans les journaux). */
int  config_validate(LithConfig *cfg);

/* Affiche la configuration effective (commande `lith check`). */
void config_print(const LithConfig *cfg);

/* Utilitaires exposés pour les tests. */
bool config_parse_size(const char *s, long long *out);
bool config_parse_bool(const char *s, bool *out);

/* Modèle de fichier lith.conf écrit par `lith init`. */
extern const char *LITH_DEFAULT_CONF;

#endif /* LITH_CONFIG_H */
