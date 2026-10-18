# LITH — Light HTTP Server

**LITH** est un serveur web léger, rapide et sûr, écrit en **C11 standard**,
**sans aucune dépendance externe**. Un seul petit exécutable (≈ 300 Ko) suffit
pour publier un site, développer en local avec rechargement automatique, ou
placer une application (Node.js, Python, Go...) derrière un proxy.

Il fonctionne à l'identique sous **Windows**, **Linux** et **macOS**.

```text
lith init mon-site
cd mon-site
lith start
→ http://localhost:8090
```

> 📘 **Nouveau venu ?** Toute l'explication, pas à pas, depuis zéro, se trouve
> dans **[DOCUMENTATION.md](DOCUMENTATION.md)**. Ce README n'en est qu'un résumé.

---

## Pourquoi LITH ?

| | |
|---|---|
| 🪶 **Minuscule** | ≈ 300 Ko, quelques Mo de RAM, image Docker de ≈ 330 Ko |
| 📦 **Zéro dépendance** | Ni OpenSSL, ni zlib, ni runtime : seulement la bibliothèque C du système |
| 🔒 **HTTPS en option** | `make TLS=1` : TLS 1.2/1.3 via un petit module Rust (rustls), redirection HTTP → HTTPS, HSTS |
| ⚡ **Rapide** | Pool de threads, keep-alive, cache mémoire, gzip, `sendfile()` |
| 🛡️ **Sûr par défaut** | Anti-remontée de dossiers, fichiers cachés protégés, en-têtes de sécurité, anti-slowloris |
| 🧰 **Complet** | Proxy inverse, répartition de charge, WebSocket, live reload, statistiques Prometheus |
| 🧪 **Testé** | Tests unitaires + intégration, sous sanitizers, en CI sur 3 systèmes + Docker |

## Fonctionnalités de la 1.0.0

- **Fichiers statiques** : cache LRU en RAM, compression **gzip**, `ETag`/`304`,
  requêtes partielles **Range** (vidéo), liste de dossiers, mode **SPA**, pages d'erreur personnalisées.
- **HTTP/1.1 solide** : keep-alive, pipelining, `100-continue`, limites de taille, codes d'erreur précis.
- **Sécurité** : limitation de débit par IP, authentification **Basic** par chemin,
  `Content-Security-Policy`, **CORS**, blocage de `.env`/`.git`.
- **Proxy inverse** : `PROXY=/api http://127.0.0.1:3000`, plusieurs backends en
  **round-robin**, bascule et **health checks** automatiques.
- **Développement** : `lith start --dev` recharge le navigateur à chaque modification (WebSocket).
- **Supervision** : `/__lith/health`, `/__lith/status` (JSON), `/__lith/metrics` (Prometheus).
- **Exploitation** : mode démon + `lith stop`, journal d'accès, arrêt propre, Docker.
- **HTTPS (option `make TLS=1`)** : `TLS_PORT`, certificats PEM (Let's Encrypt), `TLS_REDIRECT`, `HSTS`,
  `X-Forwarded-Proto: https` vers les backends. Sans l'option, le cœur reste 100 % C, sans dépendance.

---

## Installation

### Télécharger un exécutable
Prenez l'archive de votre système dans les
[Releases GitHub](https://github.com/fomadev/lith/releases), décompressez-la, et
lancez `bin/lith` (ou `bin\lith.exe`).

### Compiler soi-même
Il faut un compilateur C11 (`gcc`/`clang`, ou **MinGW-w64** sous Windows) et `make`.

```bash
git clone https://github.com/fomadev/lith.git
cd lith
make            # produit bin/lith (ou bin/lith.exe)
make test       # tests unitaires
```

Avec HTTPS (il faut en plus [Rust](https://rustup.rs) ; sous Windows avec
MinGW : `rustup target add x86_64-pc-windows-gnu`) :

```bash
make TLS=1      # ajoute le module tls/ (rustls)
make TLS=1 test
```

### Docker

```bash
docker build -t lith .
docker run -p 8090:8090 -v "$PWD/public:/srv/public:ro" lith
```

---

## Utilisation

```bash
lith start                  # démarre avec lith.conf (port 8090 par défaut)
lith start 8080             # autre port
lith start --dev            # rechargement automatique du navigateur
lith start -r ./dist        # servir un autre dossier
lith start -d               # en arrière-plan (Linux/macOS), puis : lith stop
lith check                  # vérifier la configuration
lith health                 # tester un serveur en marche
lith help                   # toutes les commandes
```

## Configuration en un coup d'œil

Tout se règle dans `lith.conf` (créé par `lith init`), au format `CLE=VALEUR` :

```ini
PORT=8090
PUBLIC_DIR=public
GZIP=1
USE_CACHE=1
RATE_LIMIT=50                      # 50 requêtes/s par IP
AUTH=/admin admin:motdepasse       # zone protégée
PROXY=/api http://127.0.0.1:3000   # envoyer /api à votre application
SPA_FALLBACK=1                     # React, Vue, Angular...

# HTTPS (exécutable compilé avec make TLS=1)
TLS_PORT=443
TLS_CERT=fullchain.pem
TLS_KEY=privkey.pem
TLS_REDIRECT=1                     # http:// -> https://
HSTS=31536000
```

La liste complète des réglages (plus de 35) est détaillée au
[chapitre 5 de la documentation](DOCUMENTATION.md#chapitre-5--le-fichier-de-configuration-lithconf).

---

## Structure du dépôt

```text
include/   en-têtes C (un par module)
src/       code source : serveur, HTTP, fichiers, proxy, WebSocket...
tls/       module HTTPS optionnel en Rust (rustls), compilé avec make TLS=1
tests/     tests unitaires (C) et d'intégration (bash + curl)
public/    site d'exemple servi par défaut
docker/    configuration de l'image Docker
lith.conf  configuration commentée
```

## Documentation

| Document | Contenu |
|----------|---------|
| **[DOCUMENTATION.md](DOCUMENTATION.md)** | Le guide complet, du chapitre 0 (« c'est quoi un serveur web ? ») jusqu'à la production, l'architecture interne et la feuille de route |
| [CHANGELOG.md](CHANGELOG.md) | Ce qui a changé dans chaque version |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Comment contribuer au code |

## Licence

Distribué sous la [FomaDev Public License (FPL)](LICENSE). L'utilisation des
exécutables compilés est gratuite, y compris en entreprise. La republication
commerciale, l'intégration du code source dans un outil propriétaire concurrent
et les forks indépendants permanents nécessitent une autorisation commerciale.
Consultez le fichier [LICENSE](LICENSE) pour les conditions exactes.
