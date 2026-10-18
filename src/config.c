/* Copyright (c) 2026 Fordima Inc. All rights reserved.
 * Licensed under the Fordima Public License (FPL) v1.0.
 * See the LICENSE file in the project root for full license information.
 */

#include "config.h"
#include "tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

LithConfig g_config;

void config_set_defaults(LithConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->host, sizeof(cfg->host), "0.0.0.0");
    cfg->port = 8090;
    cfg->threads = 0;
    cfg->max_pending = 1024;
    cfg->keepalive_timeout = 5;
    cfg->keepalive_max = 100;
    cfg->request_timeout = 10;
    cfg->max_body_size = 10LL * 1024 * 1024;

    snprintf(cfg->public_dir, sizeof(cfg->public_dir), "public");
    snprintf(cfg->index_file, sizeof(cfg->index_file), "index.html");
    cfg->auto_index = false;
    cfg->spa_fallback = false;
    cfg->hide_dotfiles = true;
    snprintf(cfg->cache_control, sizeof(cfg->cache_control), "no-cache");

    cfg->use_cache = true;
    cfg->cache_max_size = 64LL * 1024 * 1024;
    cfg->cache_file_max = 2LL * 1024 * 1024;
    cfg->gzip = true;

    cfg->security_headers = true;
    cfg->rate_limit = 0;
    cfg->rate_burst = 0;

    cfg->proxy_timeout = 30;
    cfg->health_interval = 5;

    cfg->live_reload = false;
    cfg->status_enabled = true;

    cfg->log_level = LOG_INFO;
    cfg->access_log = true;
    snprintf(cfg->pid_file, sizeof(cfg->pid_file), "lith.pid");
}

/* ---------------------------------------------------------------------- */

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

bool config_parse_bool(const char *s, bool *out) {
    if (!strcasecmp(s, "1") || !strcasecmp(s, "true") || !strcasecmp(s, "on") || !strcasecmp(s, "yes") || !strcasecmp(s, "oui")) {
        *out = true;
        return true;
    }
    if (!strcasecmp(s, "0") || !strcasecmp(s, "false") || !strcasecmp(s, "off") || !strcasecmp(s, "no") || !strcasecmp(s, "non")) {
        *out = false;
        return true;
    }
    return false;
}

/* Accepte "1048576", "512K", "10M", "1G" (insensible à la casse, "B"/"o" final toléré). */
bool config_parse_size(const char *s, long long *out) {
    if (!s || !*s) return false;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (errno != 0 || end == s || v < 0) return false;
    while (*end == ' ') end++;
    long long mult = 1;
    switch (toupper((unsigned char)*end)) {
        case 'K': mult = 1024LL; end++; break;
        case 'M': mult = 1024LL * 1024; end++; break;
        case 'G': mult = 1024LL * 1024 * 1024; end++; break;
        default: break;
    }
    if (toupper((unsigned char)*end) == 'B' || toupper((unsigned char)*end) == 'O') end++;
    if (*end != '\0') return false;
    if (v > (long long)(9e18 / (double)mult)) return false;
    *out = v * mult;
    return true;
}

static bool parse_int(const char *s, int min, int max, int *out) {
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < min || v > max) return false;
    *out = (int)v;
    return true;
}

static bool copy_str(char *dst, size_t size, const char *src) {
    if (strlen(src) >= size) return false;
    memcpy(dst, src, strlen(src) + 1);
    return true;
}

/* "http://hote:port" ou "hote:port" ou "[::1]:port" -> BackendConf */
static bool parse_backend(const char *url, BackendConf *b, char *err, size_t err_size) {
    const char *p = url;
    if (strncasecmp(p, "https://", 8) == 0) {
        snprintf(err, err_size, "les backends HTTPS ne sont pas pris en charge (%s)", url);
        return false;
    }
    if (strncasecmp(p, "http://", 7) == 0) p += 7;

    char hostport[300];
    size_t n = strcspn(p, "/");
    if (n == 0 || n >= sizeof(hostport)) {
        snprintf(err, err_size, "adresse de backend invalide: %s", url);
        return false;
    }
    memcpy(hostport, p, n);
    hostport[n] = '\0';

    const char *host_start = hostport;
    const char *port_str = "80";
    char host[256];

    if (hostport[0] == '[') {
        char *close = strchr(hostport, ']');
        if (!close) { snprintf(err, err_size, "adresse IPv6 invalide: %s", url); return false; }
        *close = '\0';
        host_start = hostport + 1;
        if (close[1] == ':') port_str = close + 2;
    } else {
        char *colon = strrchr(hostport, ':');
        if (colon) { *colon = '\0'; port_str = colon + 1; }
    }
    if (!copy_str(host, sizeof(host), host_start) || host[0] == '\0') {
        snprintf(err, err_size, "hôte de backend invalide: %s", url);
        return false;
    }
    int port;
    if (!parse_int(port_str, 1, 65535, &port)) {
        snprintf(err, err_size, "port de backend invalide: %s", url);
        return false;
    }
    snprintf(b->host, sizeof(b->host), "%s", host);
    snprintf(b->port, sizeof(b->port), "%d", port);
    return true;
}

/* Normalise un préfixe d'URL : commence par '/', pas de '/' final (sauf "/"). */
static bool normalize_prefix(const char *in, char *out, size_t size) {
    if (in[0] != '/') return false;
    if (!copy_str(out, size, in)) return false;
    size_t n = strlen(out);
    while (n > 1 && out[n - 1] == '/') out[--n] = '\0';
    return true;
}

static bool parse_proxy(LithConfig *cfg, char *value, char *err, size_t err_size) {
    if (cfg->proxy_count >= MAX_PROXY_ROUTES) {
        snprintf(err, err_size, "trop de règles PROXY (maximum %d)", MAX_PROXY_ROUTES);
        return false;
    }
    char *prefix = strtok(value, " \t");
    if (!prefix) { snprintf(err, err_size, "format attendu: PROXY=/prefixe http://hote:port"); return false; }

    ProxyRouteConf *r = &cfg->proxies[cfg->proxy_count];
    memset(r, 0, sizeof(*r));
    if (!normalize_prefix(prefix, r->prefix, sizeof(r->prefix))) {
        snprintf(err, err_size, "le préfixe PROXY doit commencer par '/' (%s)", prefix);
        return false;
    }
    char *tok;
    while ((tok = strtok(NULL, " \t,")) != NULL) {
        if (r->backend_count >= MAX_PROXY_BACKENDS) {
            snprintf(err, err_size, "trop de backends pour %s (maximum %d)", r->prefix, MAX_PROXY_BACKENDS);
            return false;
        }
        if (!parse_backend(tok, &r->backends[r->backend_count], err, err_size)) return false;
        r->backend_count++;
    }
    if (r->backend_count == 0) {
        snprintf(err, err_size, "aucun backend indiqué pour %s", r->prefix);
        return false;
    }
    cfg->proxy_count++;
    return true;
}

static bool parse_auth(LithConfig *cfg, char *value, char *err, size_t err_size) {
    if (cfg->auth_count >= MAX_AUTH_RULES) {
        snprintf(err, err_size, "trop de règles AUTH (maximum %d)", MAX_AUTH_RULES);
        return false;
    }
    char *prefix = strtok(value, " \t");
    char *cred = strtok(NULL, " \t");
    if (!prefix || !cred || !strchr(cred, ':')) {
        snprintf(err, err_size, "format attendu: AUTH=/prefixe utilisateur:motdepasse");
        return false;
    }
    AuthRuleConf *a = &cfg->auth[cfg->auth_count];
    if (!normalize_prefix(prefix, a->prefix, sizeof(a->prefix))) {
        snprintf(err, err_size, "le préfixe AUTH doit commencer par '/' (%s)", prefix);
        return false;
    }
    if (!copy_str(a->credentials, sizeof(a->credentials), cred)) {
        snprintf(err, err_size, "identifiants AUTH trop longs");
        return false;
    }
    cfg->auth_count++;
    return true;
}

#define BAD(...) do { snprintf(err, err_size, __VA_ARGS__); return false; } while (0)

bool config_apply(LithConfig *cfg, const char *key, const char *value_in, char *err, size_t err_size) {
    char value[1024];
    if (!copy_str(value, sizeof(value), value_in)) BAD("valeur trop longue pour %s", key);
    /* Réseau */
    if (!strcasecmp(key, "HOST")) {
        if (!copy_str(cfg->host, sizeof(cfg->host), value) || !value[0]) BAD("HOST invalide");
    } else if (!strcasecmp(key, "PORT")) {
        if (!parse_int(value, 1, 65535, &cfg->port)) BAD("PORT doit être un nombre entre 1 et 65535");
    } else if (!strcasecmp(key, "THREADS")) {
        if (!parse_int(value, 0, 1024, &cfg->threads)) BAD("THREADS doit être entre 0 (auto) et 1024");
    } else if (!strcasecmp(key, "MAX_PENDING")) {
        if (!parse_int(value, 1, 1000000, &cfg->max_pending)) BAD("MAX_PENDING doit être >= 1");
    } else if (!strcasecmp(key, "KEEPALIVE_TIMEOUT")) {
        if (!parse_int(value, 0, 3600, &cfg->keepalive_timeout)) BAD("KEEPALIVE_TIMEOUT doit être entre 0 et 3600");
    } else if (!strcasecmp(key, "KEEPALIVE_MAX")) {
        if (!parse_int(value, 1, 1000000, &cfg->keepalive_max)) BAD("KEEPALIVE_MAX doit être >= 1");
    } else if (!strcasecmp(key, "REQUEST_TIMEOUT")) {
        if (!parse_int(value, 1, 3600, &cfg->request_timeout)) BAD("REQUEST_TIMEOUT doit être entre 1 et 3600");
    } else if (!strcasecmp(key, "MAX_BODY_SIZE")) {
        if (!config_parse_size(value, &cfg->max_body_size)) BAD("MAX_BODY_SIZE invalide (ex: 10M)");
    }
    /* HTTPS */
    else if (!strcasecmp(key, "TLS_PORT")) {
        if (!parse_int(value, 0, 65535, &cfg->tls_port)) BAD("TLS_PORT doit être un nombre entre 0 (désactivé) et 65535");
    } else if (!strcasecmp(key, "TLS_CERT")) {
        if (!copy_str(cfg->tls_cert, sizeof(cfg->tls_cert), value)) BAD("TLS_CERT trop long");
    } else if (!strcasecmp(key, "TLS_KEY")) {
        if (!copy_str(cfg->tls_key, sizeof(cfg->tls_key), value)) BAD("TLS_KEY trop long");
    } else if (!strcasecmp(key, "TLS_REDIRECT")) {
        if (!config_parse_bool(value, &cfg->tls_redirect)) BAD("TLS_REDIRECT attend 0 ou 1");
    } else if (!strcasecmp(key, "HSTS")) {
        if (!parse_int(value, 0, 2147483647, &cfg->hsts)) BAD("HSTS attend une durée en secondes (0 = désactivé, ex: 31536000)");
    }
    /* Fichiers */
    else if (!strcasecmp(key, "PUBLIC_DIR")) {
        if (!value[0] || !copy_str(cfg->public_dir, sizeof(cfg->public_dir), value)) BAD("PUBLIC_DIR invalide");
        size_t n = strlen(cfg->public_dir);
        while (n > 1 && (cfg->public_dir[n - 1] == '/' || cfg->public_dir[n - 1] == '\\')) cfg->public_dir[--n] = '\0';
    } else if (!strcasecmp(key, "INDEX_FILE")) {
        if (!value[0] || strchr(value, '/') || strchr(value, '\\') || !copy_str(cfg->index_file, sizeof(cfg->index_file), value))
            BAD("INDEX_FILE doit être un simple nom de fichier");
    } else if (!strcasecmp(key, "AUTO_INDEX")) {
        if (!config_parse_bool(value, &cfg->auto_index)) BAD("AUTO_INDEX attend 0 ou 1");
    } else if (!strcasecmp(key, "SPA_FALLBACK")) {
        if (!config_parse_bool(value, &cfg->spa_fallback)) BAD("SPA_FALLBACK attend 0 ou 1");
    } else if (!strcasecmp(key, "HIDE_DOTFILES")) {
        if (!config_parse_bool(value, &cfg->hide_dotfiles)) BAD("HIDE_DOTFILES attend 0 ou 1");
    } else if (!strcasecmp(key, "CACHE_CONTROL")) {
        if (!copy_str(cfg->cache_control, sizeof(cfg->cache_control), value)) BAD("CACHE_CONTROL trop long");
    }
    /* Performance */
    else if (!strcasecmp(key, "USE_CACHE")) {
        if (!config_parse_bool(value, &cfg->use_cache)) BAD("USE_CACHE attend 0 ou 1");
    } else if (!strcasecmp(key, "CACHE_MAX_SIZE")) {
        if (!config_parse_size(value, &cfg->cache_max_size)) BAD("CACHE_MAX_SIZE invalide (ex: 64M)");
    } else if (!strcasecmp(key, "CACHE_FILE_MAX")) {
        if (!config_parse_size(value, &cfg->cache_file_max)) BAD("CACHE_FILE_MAX invalide (ex: 2M)");
    } else if (!strcasecmp(key, "GZIP")) {
        if (!config_parse_bool(value, &cfg->gzip)) BAD("GZIP attend 0 ou 1");
    }
    /* Sécurité */
    else if (!strcasecmp(key, "SECURITY_HEADERS")) {
        if (!config_parse_bool(value, &cfg->security_headers)) BAD("SECURITY_HEADERS attend 0 ou 1");
    } else if (!strcasecmp(key, "CSP")) {
        if (!copy_str(cfg->csp, sizeof(cfg->csp), value)) BAD("CSP trop longue");
    } else if (!strcasecmp(key, "CORS_ORIGIN")) {
        if (!copy_str(cfg->cors_origin, sizeof(cfg->cors_origin), value)) BAD("CORS_ORIGIN trop long");
    } else if (!strcasecmp(key, "RATE_LIMIT")) {
        if (!parse_int(value, 0, 1000000, &cfg->rate_limit)) BAD("RATE_LIMIT doit être >= 0");
    } else if (!strcasecmp(key, "RATE_BURST")) {
        if (!parse_int(value, 0, 1000000, &cfg->rate_burst)) BAD("RATE_BURST doit être >= 0");
    } else if (!strcasecmp(key, "AUTH")) {
        return parse_auth(cfg, value, err, err_size);
    }
    /* Proxy */
    else if (!strcasecmp(key, "PROXY")) {
        return parse_proxy(cfg, value, err, err_size);
    } else if (!strcasecmp(key, "PROXY_TIMEOUT")) {
        if (!parse_int(value, 1, 3600, &cfg->proxy_timeout)) BAD("PROXY_TIMEOUT doit être entre 1 et 3600");
    } else if (!strcasecmp(key, "HEALTH_CHECK_INTERVAL")) {
        if (!parse_int(value, 0, 3600, &cfg->health_interval)) BAD("HEALTH_CHECK_INTERVAL doit être entre 0 et 3600");
    }
    /* Dev & supervision */
    else if (!strcasecmp(key, "LIVE_RELOAD")) {
        if (!config_parse_bool(value, &cfg->live_reload)) BAD("LIVE_RELOAD attend 0 ou 1");
    } else if (!strcasecmp(key, "STATUS")) {
        if (!config_parse_bool(value, &cfg->status_enabled)) BAD("STATUS attend 0 ou 1");
    } else if (!strcasecmp(key, "STATUS_AUTH")) {
        if (value[0] && !strchr(value, ':')) BAD("STATUS_AUTH attend utilisateur:motdepasse");
        if (!copy_str(cfg->status_auth, sizeof(cfg->status_auth), value)) BAD("STATUS_AUTH trop long");
    }
    /* Journaux */
    else if (!strcasecmp(key, "LOG_LEVEL")) {
        if (!lith_log_level_parse(value, &cfg->log_level)) BAD("LOG_LEVEL attend debug, info, warn ou error");
    } else if (!strcasecmp(key, "LOG_FILE")) {
        if (!copy_str(cfg->log_file, sizeof(cfg->log_file), value)) BAD("LOG_FILE trop long");
    } else if (!strcasecmp(key, "ACCESS_LOG")) {
        if (!config_parse_bool(value, &cfg->access_log)) BAD("ACCESS_LOG attend 0 ou 1");
    } else if (!strcasecmp(key, "PID_FILE")) {
        if (!copy_str(cfg->pid_file, sizeof(cfg->pid_file), value)) BAD("PID_FILE trop long");
    } else {
        BAD("clé inconnue '%s'", key);
    }
    return true;
}

int config_load(const char *path, LithConfig *cfg) {
    FILE *f = lith_fopen(path, "r");
    if (!f) return 1;

    char line[2048];
    int line_no = 0;
    int errors = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        /* Retire un éventuel BOM UTF-8 sur la première ligne */
        char *p = line;
        if (line_no == 1 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
        p = trim(p);
        if (*p == '\0' || *p == '#' || *p == ';') continue;

        char *eq = strchr(p, '=');
        if (!eq) {
            lith_log(LOG_ERROR, "%s:%d : ligne ignorée, '=' manquant", path, line_no);
            errors++;
            continue;
        }
        *eq = '\0';
        char *key = trim(p);
        char *value = trim(eq + 1);
        /* Guillemets optionnels autour de la valeur */
        size_t vl = strlen(value);
        if (vl >= 2 && ((value[0] == '"' && value[vl - 1] == '"') || (value[0] == '\'' && value[vl - 1] == '\''))) {
            value[vl - 1] = '\0';
            value++;
        }

        char err[256];
        if (!config_apply(cfg, key, value, err, sizeof(err))) {
            lith_log(LOG_ERROR, "%s:%d : %s", path, line_no, err);
            errors++;
        }
    }
    fclose(f);
    return errors ? -1 : 0;
}

int config_validate(LithConfig *cfg) {
    int errors = 0;
    lith_stat_t st;
    if (lith_stat(cfg->public_dir, &st) != 0 || !st.is_dir) {
        lith_log(LOG_ERROR, "Le dossier public '%s' est introuvable. Créez-le ou corrigez PUBLIC_DIR (astuce : `lith init`).", cfg->public_dir);
        errors++;
    }
    if (cfg->tls_port > 0) {
        char err[512];
        if (!tls_compiled()) {
            lith_log(LOG_ERROR, "TLS_PORT=%d : cet exécutable a été compilé sans HTTPS. Recompilez avec `make TLS=1`.", cfg->tls_port);
            errors++;
        } else if (!cfg->tls_cert[0] || !cfg->tls_key[0]) {
            lith_log(LOG_ERROR, "TLS_PORT=%d : indiquez le certificat (TLS_CERT) et la clé privée (TLS_KEY).", cfg->tls_port);
            errors++;
        } else if (!tls_check_files(cfg->tls_cert, cfg->tls_key, err, sizeof(err))) {
            lith_log(LOG_ERROR, "HTTPS : %s", err);
            errors++;
        }
        if (cfg->tls_port == cfg->port) {
            lith_log(LOG_ERROR, "PORT et TLS_PORT doivent être différents (%d)", cfg->port);
            errors++;
        }
    } else if (cfg->tls_redirect) {
        lith_log(LOG_WARN, "TLS_REDIRECT ignoré : HTTPS n'est pas activé (TLS_PORT=0).");
        cfg->tls_redirect = false;
    }
    if (cfg->cache_file_max > cfg->cache_max_size) cfg->cache_file_max = cfg->cache_max_size;
    if (cfg->rate_limit > 0 && cfg->rate_burst < cfg->rate_limit) cfg->rate_burst = cfg->rate_limit * 2;
    for (int i = 0; i < cfg->proxy_count; i++) {
        if (strncmp(cfg->proxies[i].prefix, "/__lith", 7) == 0) {
            lith_log(LOG_ERROR, "Le préfixe PROXY %s est réservé à LITH", cfg->proxies[i].prefix);
            errors++;
        }
    }
    if (cfg->live_reload && cfg->use_cache) {
        lith_log(LOG_DEBUG, "LIVE_RELOAD actif : les pages HTML ne passent pas par le cache.");
    }
    return errors;
}

void config_print(const LithConfig *c) {
    printf("Configuration effective de LITH %s\n", LITH_VERSION);
    printf("  HOST                  = %s\n", c->host);
    printf("  PORT                  = %d\n", c->port);
    printf("  THREADS               = %d%s\n", c->threads, c->threads == 0 ? " (auto)" : "");
    printf("  MAX_PENDING           = %d\n", c->max_pending);
    printf("  KEEPALIVE_TIMEOUT     = %d s\n", c->keepalive_timeout);
    printf("  KEEPALIVE_MAX         = %d\n", c->keepalive_max);
    printf("  REQUEST_TIMEOUT       = %d s\n", c->request_timeout);
    printf("  MAX_BODY_SIZE         = %lld octets\n", c->max_body_size);
    printf("  TLS_PORT              = %d%s\n", c->tls_port,
           c->tls_port ? "" : (tls_compiled() ? " (HTTPS désactivé)" : " (exécutable compilé sans HTTPS : make TLS=1)"));
    if (c->tls_port) {
        printf("  TLS_CERT              = %s\n", c->tls_cert[0] ? c->tls_cert : "(manquant)");
        printf("  TLS_KEY               = %s\n", c->tls_key[0] ? c->tls_key : "(manquante)");
        printf("  TLS_REDIRECT          = %d\n", c->tls_redirect);
        printf("  HSTS                  = %d s%s\n", c->hsts, c->hsts ? "" : " (désactivé)");
    }
    printf("  PUBLIC_DIR            = %s\n", c->public_dir);
    printf("  INDEX_FILE            = %s\n", c->index_file);
    printf("  AUTO_INDEX            = %d\n", c->auto_index);
    printf("  SPA_FALLBACK          = %d\n", c->spa_fallback);
    printf("  HIDE_DOTFILES         = %d\n", c->hide_dotfiles);
    printf("  CACHE_CONTROL         = %s\n", c->cache_control);
    printf("  USE_CACHE             = %d\n", c->use_cache);
    printf("  CACHE_MAX_SIZE        = %lld octets\n", c->cache_max_size);
    printf("  CACHE_FILE_MAX        = %lld octets\n", c->cache_file_max);
    printf("  GZIP                  = %d\n", c->gzip);
    printf("  SECURITY_HEADERS      = %d\n", c->security_headers);
    printf("  CSP                   = %s\n", c->csp[0] ? c->csp : "(aucune)");
    printf("  CORS_ORIGIN           = %s\n", c->cors_origin[0] ? c->cors_origin : "(désactivé)");
    printf("  RATE_LIMIT            = %d req/s%s\n", c->rate_limit, c->rate_limit ? "" : " (désactivé)");
    printf("  RATE_BURST            = %d\n", c->rate_burst);
    for (int i = 0; i < c->auth_count; i++)
        printf("  AUTH                  = %s (utilisateur: %.*s)\n", c->auth[i].prefix,
               (int)strcspn(c->auth[i].credentials, ":"), c->auth[i].credentials);
    for (int i = 0; i < c->proxy_count; i++) {
        printf("  PROXY                 = %s ->", c->proxies[i].prefix);
        for (int j = 0; j < c->proxies[i].backend_count; j++)
            printf(" %s:%s", c->proxies[i].backends[j].host, c->proxies[i].backends[j].port);
        printf("\n");
    }
    printf("  PROXY_TIMEOUT         = %d s\n", c->proxy_timeout);
    printf("  HEALTH_CHECK_INTERVAL = %d s\n", c->health_interval);
    printf("  LIVE_RELOAD           = %d\n", c->live_reload);
    printf("  STATUS                = %d\n", c->status_enabled);
    printf("  STATUS_AUTH           = %s\n", c->status_auth[0] ? "(défini)" : "(aucun : accès local uniquement)");
    printf("  LOG_LEVEL             = %s\n", lith_log_level_name(c->log_level));
    printf("  LOG_FILE              = %s\n", c->log_file[0] ? c->log_file : "(console)");
    printf("  ACCESS_LOG            = %d\n", c->access_log);
    printf("  PID_FILE              = %s\n", c->pid_file);
}

const char *LITH_DEFAULT_CONF =
"# ==============================================================\n"
"#  LITH 1.0.0 - Fichier de configuration\n"
"#  Format : CLE=VALEUR  (une par ligne, '#' = commentaire)\n"
"#  Toutes les clés sont optionnelles : la valeur par défaut est\n"
"#  indiquée en commentaire. Voir DOCUMENTATION.md, chapitre 5.\n"
"# ==============================================================\n"
"\n"
"# --- Réseau ---------------------------------------------------\n"
"# Adresse d'écoute (0.0.0.0 = toutes les interfaces IPv4, :: = IPv4+IPv6)\n"
"HOST=0.0.0.0\n"
"# Port d'écoute\n"
"PORT=8090\n"
"# Nombre de threads de travail (0 = automatique)\n"
"THREADS=0\n"
"# Durée (s) pendant laquelle une connexion inactive reste ouverte\n"
"KEEPALIVE_TIMEOUT=5\n"
"# Temps maximum (s) pour recevoir une requête complète\n"
"REQUEST_TIMEOUT=10\n"
"# Taille maximum du corps d'une requête (K, M, G acceptés)\n"
"MAX_BODY_SIZE=10M\n"
"\n"
"# --- HTTPS (exécutable compilé avec : make TLS=1) ---------------\n"
"# Port HTTPS (0 = désactivé ; le port HTTP ci-dessus reste ouvert)\n"
"# TLS_PORT=8443\n"
"# Certificat (chaîne complète, PEM) et clé privée (PEM)\n"
"# TLS_CERT=certs/fullchain.pem\n"
"# TLS_KEY=certs/privkey.pem\n"
"# Rediriger le port HTTP vers HTTPS (sauf /__lith/health et /.well-known/acme-challenge/)\n"
"# TLS_REDIRECT=1\n"
"# Strict-Transport-Security : durée en secondes (0 = non envoyé)\n"
"# HSTS=31536000\n"
"\n"
"# --- Fichiers -------------------------------------------------\n"
"# Dossier contenant le site web\n"
"PUBLIC_DIR=public\n"
"# Fichier servi quand on demande un dossier\n"
"INDEX_FILE=index.html\n"
"# Afficher la liste des fichiers d'un dossier sans index.html (0/1)\n"
"AUTO_INDEX=0\n"
"# Applications monopage (React, Vue...) : renvoyer index.html pour les routes inconnues\n"
"SPA_FALLBACK=0\n"
"# Cacher les fichiers commençant par un point (.env, .git...)\n"
"HIDE_DOTFILES=1\n"
"\n"
"# --- Performance ----------------------------------------------\n"
"# Garder les petits fichiers en mémoire vive (rechargés si modifiés)\n"
"USE_CACHE=1\n"
"CACHE_MAX_SIZE=64M\n"
"CACHE_FILE_MAX=2M\n"
"# Compression gzip des fichiers texte (HTML, CSS, JS, JSON, SVG...)\n"
"GZIP=1\n"
"\n"
"# --- Sécurité -------------------------------------------------\n"
"# En-têtes de protection (nosniff, anti-clickjacking, referrer)\n"
"SECURITY_HEADERS=1\n"
"# Content-Security-Policy (vide = non envoyée)\n"
"# CSP=default-src 'self'\n"
"# Autoriser les appels depuis un autre site (CORS). Vide = désactivé\n"
"# CORS_ORIGIN=*\n"
"# Limite de requêtes par seconde et par adresse IP (0 = désactivé)\n"
"RATE_LIMIT=0\n"
"# Protéger un chemin par mot de passe (répétable)\n"
"# AUTH=/admin admin:changez-moi\n"
"\n"
"# --- Proxy inverse ---------------------------------------------\n"
"# Transmettre un préfixe d'URL à une autre application (répétable).\n"
"# Plusieurs backends = répartition de charge (round-robin).\n"
"# PROXY=/api http://127.0.0.1:3000\n"
"# PROXY=/app http://127.0.0.1:4000,http://127.0.0.1:4001\n"
"PROXY_TIMEOUT=30\n"
"HEALTH_CHECK_INTERVAL=5\n"
"\n"
"# --- Développement & supervision -------------------------------\n"
"# Recharger automatiquement le navigateur quand un fichier change\n"
"LIVE_RELOAD=0\n"
"# Pages /__lith/status et /__lith/metrics (accès local uniquement sans STATUS_AUTH)\n"
"STATUS=1\n"
"# STATUS_AUTH=admin:changez-moi\n"
"\n"
"# --- Journaux ---------------------------------------------------\n"
"# Niveau : debug, info, warn, error\n"
"LOG_LEVEL=info\n"
"# Fichier de journal (vide = console)\n"
"# LOG_FILE=lith.log\n"
"# Une ligne par requête reçue\n"
"ACCESS_LOG=1\n"
"# Fichier PID du mode démon (Linux/macOS)\n"
"PID_FILE=lith.pid\n";
