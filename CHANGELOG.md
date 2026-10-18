# Journal des modifications

Toutes les évolutions notables de LITH sont listées ici.
Le projet suit le [versionnage sémantique](https://semver.org/lang/fr/) : `MAJEUR.MINEUR.CORRECTIF`.

## [Non publié]

### Ajouté
- **HTTPS natif, en option de compilation** : `make TLS=1`. Le chiffrement est
  confié à un petit module Rust (`tls/`, bibliothèque **rustls**, fournisseur
  cryptographique *ring*) compilé en bibliothèque statique et appelé via une
  API C minimale (`tls/lith_tls.h`). Sans `TLS=1`, l'exécutable reste 100 % C,
  sans aucune dépendance, et son comportement est inchangé.
- Nouvelles clés de configuration : `TLS_PORT` (port HTTPS, en plus de `PORT`),
  `TLS_CERT` / `TLS_KEY` (certificat et clé PEM), `TLS_REDIRECT` (redirection
  `301`/`308` de HTTP vers HTTPS, sauf `/__lith/health` et
  `/.well-known/acme-challenge/`) et `HSTS` (`Strict-Transport-Security`).
- TLS 1.2 et 1.3, ALPN `http/1.1`, reprise de session ; toutes les fonctions
  (fichiers, gzip, Range, keep-alive, proxy, live reload en `wss://`,
  statistiques) fonctionnent en HTTPS, avec les mêmes délais anti-slowloris,
  poignée de main comprise.
- `lith check` charge réellement le certificat et la clé et explique les erreurs ;
  un exécutable compilé sans HTTPS refuse `TLS_PORT` au lieu de l'ignorer.
- Tests HTTPS d'intégration (certificat auto-signé généré à la volée, jamais
  stocké dans le dépôt), tests unitaires de configuration et du proxy ; la CI
  compile et teste `make TLS=1` sous Linux (avec sanitizers) et Windows.
- `make distclean` supprime aussi la compilation Rust.

### Modifié
- Proxy inverse : `X-Forwarded-Proto` vaut `https` quand le visiteur est
  arrivé en HTTPS (toujours `http` auparavant).
- Interne : toutes les E/S d'une connexion cliente passent par `LithStream`
  (`src/tls.c`), qui choisit entre la socket brute et la session TLS.
- Passer de `make` à `make TLS=1` (et inversement) refait l'édition de liens
  sans `make clean` ; les objets TLS sont rangés dans `build/tls/`.

## [1.0.0] - 2026-10-18

Première version stable. Le moteur a été entièrement restructuré en modules
indépendants, testés sous Windows, Linux et macOS, toujours sans aucune
dépendance externe.

### Serveur HTTP
- Analyseur HTTP/1.0 et HTTP/1.1 strict : méthodes, en-têtes (64 max, 16 Ko max),
  `Host` obligatoire en 1.1, décodage `%XX`, rejet des requêtes ambiguës
  (`Content-Length` + `Transfer-Encoding`, en-têtes dupliqués, repli de ligne).
- Keep-alive, pipelining, `Expect: 100-continue`, corps de requête jusqu'à `MAX_BODY_SIZE`.
- Codes d'erreur précis : 400, 401, 403, 404, 405, 408, 411, 413, 414, 416, 429, 431, 501, 502, 503, 504, 505.
- Pool de threads dimensionné automatiquement, file d'attente bornée (`MAX_PENDING`),
  libération des connexions inactives sous forte charge.
- Arrêt propre sur Ctrl+C / `SIGTERM` / `docker stop`.
- Écoute IPv4 et IPv6 (`HOST=::`).

### Fichiers statiques
- Envoi par blocs (fichiers de toute taille), `sendfile()` sous Linux.
- `ETag` / `Last-Modified` et réponses `304 Not Modified`.
- Requêtes partielles `Range` (lecture vidéo/audio, reprise de téléchargement).
- Compression gzip intégrée (implémentation maison, sans zlib).
- Cache mémoire LRU avec rechargement automatique des fichiers modifiés.
- 40 types MIME, noms de fichiers accentués (UTF-8) y compris sous Windows.
- Liste des fichiers d'un dossier (`AUTO_INDEX`), mode application monopage
  (`SPA_FALLBACK`), pages d'erreur personnalisées (`public/404.html`...).

### Sécurité
- Protection contre la remontée de dossiers (y compris encodée et pièges Windows).
- Fichiers cachés (`.env`, `.git`...) invisibles par défaut.
- En-têtes de sécurité, `Content-Security-Policy` et CORS configurables.
- Limitation de débit par adresse IP (`RATE_LIMIT`, réponse 429).
- Authentification HTTP Basic par chemin (`AUTH`).
- Protection anti-slowloris (délais de lecture stricts).

### Fonctions avancées
- Proxy inverse (`PROXY`) avec répartition de charge round-robin,
  bascule automatique et vérification périodique des backends.
- WebSocket (RFC 6455) et rechargement automatique du navigateur (`--dev`).
- Supervision : `/__lith/health`, `/__lith/status` (JSON), `/__lith/metrics` (Prometheus).
- Journal d'accès, niveaux de log, fichier de log.

### Outils
- Nouvelle ligne de commande : `start`, `stop`, `init`, `check`, `health`, `version`, `help`.
- Mode démon avec fichier PID (Linux/macOS).
- Image Docker « scratch » d'environ 330 Ko avec health check.
- Tests unitaires (`make test`) et d'intégration (`make integration`),
  intégration continue Linux/macOS/Windows/Docker avec sanitizers.
- Documentation complète pour débutants : `DOCUMENTATION.md`.
