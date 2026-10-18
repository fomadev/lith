/* Copyright (c) 2026 Fordi / FomaDev.
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

/*
 * main.c — Interface en ligne de commande de LITH.
 *
 *   lith start [PORT] [options]   démarrer le serveur
 *   lith stop                     arrêter le démon (Linux/macOS)
 *   lith init [DOSSIER]           créer lith.conf + public/index.html
 *   lith check                    vérifier la configuration
 *   lith health [PORT]            tester un serveur en marche (Docker, scripts)
 *   lith version | help
 */

#include "config.h"
#include "daemon.h"
#include "logger.h"
#include "platform.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static const char *DEFAULT_INDEX_HTML =
"<!DOCTYPE html>\n"
"<html lang=\"fr\">\n"
"<head>\n"
"  <meta charset=\"UTF-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"  <title>Mon site sur LITH</title>\n"
"  <style>\n"
"    body { font-family: system-ui, sans-serif; background: #0f172a; color: #f8fafc;\n"
"           display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }\n"
"    main { text-align: center; padding: 2rem; }\n"
"    h1 { color: #10b981; font-size: 2.5rem; margin: 0 0 .5rem; }\n"
"    p { color: #94a3b8; }\n"
"    code { background: #1e293b; padding: .15rem .4rem; border-radius: 4px; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <main>\n"
"    <h1>Ça marche !</h1>\n"
"    <p>Votre site est servi par LITH. Modifiez <code>public/index.html</code> pour commencer.</p>\n"
"  </main>\n"
"</body>\n"
"</html>\n";

static void print_usage(void) {
    printf(
        "LITH %s - serveur HTTP léger, rapide et sans dépendance\n"
        "\n"
        "Utilisation : lith <commande> [options]\n"
        "\n"
        "Commandes :\n"
        "  start [PORT]       Démarre le serveur (port optionnel)\n"
        "  stop               Arrête le serveur lancé en mode démon (Linux/macOS)\n"
        "  init [DOSSIER]     Crée un projet : lith.conf + public/index.html\n"
        "  check              Vérifie la configuration et affiche les valeurs utilisées\n"
        "  health [PORT]      Vérifie qu'un serveur répond (code de sortie 0 = OK)\n"
        "  version            Affiche la version\n"
        "  help               Affiche cette aide\n"
        "\n"
        "Options :\n"
        "  -c, --config FICHIER   Fichier de configuration (défaut : lith.conf)\n"
        "  -p, --port PORT        Port d'écoute (prioritaire sur lith.conf)\n"
        "  -H, --host ADRESSE     Adresse d'écoute (ex : 127.0.0.1, 0.0.0.0, ::)\n"
        "  -r, --root DOSSIER     Dossier des fichiers à servir\n"
        "  -d, --daemon           Lance en arrière-plan (Linux/macOS)\n"
        "      --dev              Mode développement : rechargement auto du navigateur\n"
        "  -q, --quiet            N'affiche pas une ligne par requête\n"
        "\n"
        "Exemples :\n"
        "  lith init mon-site && cd mon-site && lith start\n"
        "  lith start 8080 --dev\n"
        "  lith start -r ./dist -p 3000\n"
        "  lith start -d && lith stop\n"
        "\n"
        "Documentation complète : DOCUMENTATION.md\n",
        LITH_VERSION);
}

/* ---------------------------------------------------------------------- */

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT ||
        type == CTRL_SHUTDOWN_EVENT || type == CTRL_LOGOFF_EVENT) {
        server_request_stop();
        if (type == CTRL_CLOSE_EVENT) Sleep(3000);   /* laisse le temps de finir proprement */
        return TRUE;
    }
    return FALSE;
}
#else
static void on_signal(int sig) {
    (void)sig;
    server_request_stop();
}
#endif

static void install_signal_handlers(void) {
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
}

/* ---------------------------------------------------------------------- */

typedef struct {
    const char *config_path;
    bool        config_explicit;
    int         port;
    const char *host;
    const char *root;
    bool        daemon;
    bool        dev;
    bool        quiet;
    const char *positional;
} CliOptions;

static bool is_number(const char *s) {
    if (!s || !*s) return false;
    for (; *s; s++) if (*s < '0' || *s > '9') return false;
    return true;
}

static int parse_options(int argc, char **argv, int start, CliOptions *o) {
    memset(o, 0, sizeof(*o));
    o->config_path = "lith.conf";
    for (int i = start; i < argc; i++) {
        const char *a = argv[i];
#define NEED_VALUE() do { if (i + 1 >= argc) { fprintf(stderr, "Erreur : l'option %s attend une valeur.\n", a); return -1; } } while (0)
        if (!strcmp(a, "-c") || !strcmp(a, "--config")) {
            NEED_VALUE();
            o->config_path = argv[++i];
            o->config_explicit = true;
        } else if (!strcmp(a, "-p") || !strcmp(a, "--port")) {
            NEED_VALUE();
            if (!is_number(argv[i + 1])) { fprintf(stderr, "Erreur : port invalide '%s'.\n", argv[i + 1]); return -1; }
            o->port = atoi(argv[++i]);
        } else if (!strcmp(a, "-H") || !strcmp(a, "--host")) {
            NEED_VALUE();
            o->host = argv[++i];
        } else if (!strcmp(a, "-r") || !strcmp(a, "--root")) {
            NEED_VALUE();
            o->root = argv[++i];
        } else if (!strcmp(a, "-d") || !strcmp(a, "--daemon")) {
            o->daemon = true;
        } else if (!strcmp(a, "--dev")) {
            o->dev = true;
        } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
            o->quiet = true;
        } else if (a[0] == '-') {
            fprintf(stderr, "Erreur : option inconnue '%s'. Voir `lith help`.\n", a);
            return -1;
        } else if (!o->positional) {
            o->positional = a;
        } else {
            fprintf(stderr, "Erreur : argument en trop '%s'.\n", a);
            return -1;
        }
#undef NEED_VALUE
    }
    return 0;
}

/* Charge lith.conf puis applique les options de la ligne de commande. Retourne 0 si tout va bien. */
static int build_config(const CliOptions *o, bool positional_is_port) {
    config_set_defaults(&g_config);
    int rc = config_load(o->config_path, &g_config);
    if (rc == 1) {
        if (o->config_explicit) {
            lith_log(LOG_ERROR, "Fichier de configuration introuvable : %s", o->config_path);
            return 1;
        }
        lith_log(LOG_INFO, "Pas de lith.conf ici : valeurs par défaut utilisées (astuce : `lith init`).");
    } else if (rc < 0) {
        lith_log(LOG_ERROR, "Corrigez les erreurs de %s ci-dessus puis relancez.", o->config_path);
        return 1;
    }

    char err[256];
    if (positional_is_port && o->positional) {
        if (!config_apply(&g_config, "PORT", o->positional, err, sizeof(err))) {
            lith_log(LOG_ERROR, "Argument '%s' : %s", o->positional, err);
            return 1;
        }
    }
    if (o->port) {
        char v[16];
        snprintf(v, sizeof(v), "%d", o->port);
        if (!config_apply(&g_config, "PORT", v, err, sizeof(err))) { lith_log(LOG_ERROR, "%s", err); return 1; }
    }
    if (o->host && !config_apply(&g_config, "HOST", o->host, err, sizeof(err))) { lith_log(LOG_ERROR, "%s", err); return 1; }
    if (o->root && !config_apply(&g_config, "PUBLIC_DIR", o->root, err, sizeof(err))) { lith_log(LOG_ERROR, "%s", err); return 1; }
    if (o->dev) {
        g_config.live_reload = true;
        snprintf(g_config.cache_control, sizeof(g_config.cache_control), "no-store");
    }
    if (o->quiet) g_config.access_log = false;
    return 0;
}

#ifndef _WIN32
/* Transforme un chemin relatif en chemin absolu (nécessaire avant de passer en démon). */
static void make_absolute(char *path, size_t size, const char *cwd) {
    if (!path[0] || lith_path_is_absolute(path)) return;
    char tmp[LITH_PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s/%s", cwd, path);
    snprintf(path, size, "%s", tmp);
}
#endif

/* ---------------------------------------------------------------------- */

static int cmd_start(int argc, char **argv) {
    CliOptions o;
    if (parse_options(argc, argv, 2, &o) != 0) return 2;
    if (o.positional && !is_number(o.positional)) {
        fprintf(stderr, "Erreur : '%s' n'est pas un numéro de port valide.\n", o.positional);
        return 2;
    }
    lith_log_init(LOG_INFO, NULL);
    if (build_config(&o, true) != 0) return 1;

    if (lith_log_init(g_config.log_level, g_config.log_file) != 0) {
        fprintf(stderr, "Impossible d'ouvrir le fichier de journal '%s'.\n", g_config.log_file);
        return 1;
    }
    if (config_validate(&g_config) != 0) return 1;

    bool daemon_mode = false;
#ifndef _WIN32
    if (o.daemon) {
        char cwd[LITH_PATH_MAX];
        if (lith_getcwd(cwd, sizeof(cwd)) != 0) {
            lith_log(LOG_ERROR, "Impossible de lire le dossier courant");
            return 1;
        }
        if (!g_config.log_file[0]) snprintf(g_config.log_file, sizeof(g_config.log_file), "lith.log");
        make_absolute(g_config.public_dir, sizeof(g_config.public_dir), cwd);
        make_absolute(g_config.log_file, sizeof(g_config.log_file), cwd);
        make_absolute(g_config.pid_file, sizeof(g_config.pid_file), cwd);

        long old = lith_pidfile_read(g_config.pid_file);
        if (old > 0 && kill((pid_t)old, 0) == 0) {
            lith_log(LOG_ERROR, "LITH tourne déjà en arrière-plan (PID %ld). Utilisez `lith stop` d'abord.", old);
            return 1;
        }
        printf("LITH démarre en arrière-plan sur le port %d.\n", g_config.port);
        printf("  Journal : %s\n  PID     : %s\n  Arrêt   : lith stop\n", g_config.log_file, g_config.pid_file);
        if (lith_daemonize() != 0) {
            lith_log(LOG_ERROR, "Le passage en arrière-plan a échoué");
            return 1;
        }
        if (lith_log_init(g_config.log_level, g_config.log_file) != 0) return 1;
        if (!lith_pidfile_write(g_config.pid_file))
            lith_log(LOG_WARN, "Impossible d'écrire le fichier PID %s", g_config.pid_file);
        daemon_mode = true;
    }
#else
    if (o.daemon) lith_log(LOG_WARN, "Le mode démon (-d) n'existe pas sous Windows : LITH reste au premier plan.");
#endif

    install_signal_handlers();
    if (g_config.live_reload) lith_log(LOG_INFO, "Mode développement : le navigateur se rechargera à chaque modification.");
    int rc = server_run();
    if (daemon_mode) lith_pidfile_remove(g_config.pid_file);
    lith_log_close();
    return rc;
}

static int cmd_stop(int argc, char **argv) {
    CliOptions o;
    if (parse_options(argc, argv, 2, &o) != 0) return 2;
    lith_log_init(LOG_WARN, NULL);
    config_set_defaults(&g_config);
    if (config_load(o.config_path, &g_config) < 0) return 1;
    return lith_stop_daemon(g_config.pid_file);
}

static int cmd_check(int argc, char **argv) {
    CliOptions o;
    if (parse_options(argc, argv, 2, &o) != 0) return 2;
    lith_log_init(LOG_INFO, NULL);
    if (build_config(&o, true) != 0) return 1;
    int errors = config_validate(&g_config);
    config_print(&g_config);
    if (errors) {
        printf("\n%d erreur(s) trouvée(s).\n", errors);
        return 1;
    }
    printf("\nConfiguration valide. Lancez `lith start`.\n");
    return 0;
}

static bool write_file_if_absent(const char *path, const char *content) {
    lith_stat_t st;
    if (lith_stat(path, &st) == 0) {
        printf("  existe déjà : %s (conservé)\n", path);
        return true;
    }
    FILE *f = lith_fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "  impossible de créer %s\n", path);
        return false;
    }
    fputs(content, f);
    fclose(f);
    printf("  créé        : %s\n", path);
    return true;
}

static int cmd_init(int argc, char **argv) {
    CliOptions o;
    if (parse_options(argc, argv, 2, &o) != 0) return 2;
    const char *dir = o.positional ? o.positional : ".";
    if (o.positional && !lith_mkdir(dir)) {
        fprintf(stderr, "Impossible de créer le dossier '%s'.\n", dir);
        return 1;
    }
    char path[LITH_PATH_MAX];
    printf("Initialisation d'un projet LITH dans '%s' :\n", dir);
    snprintf(path, sizeof(path), "%s/public", dir);
    if (!lith_mkdir(path)) {
        fprintf(stderr, "Impossible de créer '%s'.\n", path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/lith.conf", dir);
    bool ok = write_file_if_absent(path, LITH_DEFAULT_CONF);
    snprintf(path, sizeof(path), "%s/public/index.html", dir);
    ok = write_file_if_absent(path, DEFAULT_INDEX_HTML) && ok;
    if (!ok) return 1;
    printf("\nC'est prêt ! Pour lancer le serveur :\n");
    if (o.positional && strcmp(dir, ".") != 0) printf("  cd %s\n", dir);
    printf("  lith start\npuis ouvrez http://localhost:8090 dans votre navigateur.\n");
    return 0;
}

static int cmd_health(int argc, char **argv) {
    CliOptions o;
    if (parse_options(argc, argv, 2, &o) != 0) return 2;
    lith_log_init(LOG_ERROR, NULL);
    config_set_defaults(&g_config);
    config_load(o.config_path, &g_config);
    int port = g_config.port;
    if (o.positional && is_number(o.positional)) port = atoi(o.positional);
    if (o.port) port = o.port;
    const char *host = o.host ? o.host : "127.0.0.1";

    if (lith_net_init() != 0) return 1;
    char port_s[8];
    snprintf(port_s, sizeof(port_s), "%d", port);
    socket_t s;
    int rc = 1;
    if (lith_connect_tcp(host, port_s, 3000, &s) == 0) {
        lith_socket_set_recv_timeout(s, 3000);
        char req[256];
        int n = snprintf(req, sizeof(req), "GET /__lith/health HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
        char resp[256] = { 0 };
        if (lith_send_all(s, req, (size_t)n) == 0 && lith_recv(s, resp, sizeof(resp) - 1) > 0 &&
            strncmp(resp, "HTTP/1.1 200", 12) == 0)
            rc = 0;
        lith_socket_close(s);
    }
    lith_net_cleanup();
    printf("%s (http://%s:%d)\n", rc == 0 ? "OK" : "INJOIGNABLE", host, port);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help")) {
        print_usage();
        return 0;
    }
    if (!strcmp(cmd, "version") || !strcmp(cmd, "-v") || !strcmp(cmd, "--version")) {
        printf("LITH %s\n", LITH_VERSION);
        return 0;
    }
    if (!strcmp(cmd, "start"))  return cmd_start(argc, argv);
    if (!strcmp(cmd, "stop"))   return cmd_stop(argc, argv);
    if (!strcmp(cmd, "init"))   return cmd_init(argc, argv);
    if (!strcmp(cmd, "check"))  return cmd_check(argc, argv);
    if (!strcmp(cmd, "health")) return cmd_health(argc, argv);

    fprintf(stderr, "Commande inconnue : '%s'\n\n", cmd);
    print_usage();
    return 2;
}
