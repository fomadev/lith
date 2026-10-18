# Journal des modifications

Toutes les évolutions notables de LITH sont listées ici.
Le projet suit le [versionnage sémantique](https://semver.org/lang/fr/) : `MAJEUR.MINEUR.CORRECTIF`.

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
