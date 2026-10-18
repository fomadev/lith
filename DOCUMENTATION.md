# Documentation de LITH 1.0.0

> Le guide complet de LITH, écrit pour être compris **même si vous n'avez
> jamais installé de serveur web**. Chaque notion est expliquée avant d'être
> utilisée. Les développeurs expérimentés peuvent sauter directement aux
> chapitres qui les intéressent.

---

## Sommaire

- [Chapitre 0 — Introduction](#chapitre-0--introduction)
- [Chapitre 1 — Les notions de base du web](#chapitre-1--les-notions-de-base-du-web)
- [Chapitre 2 — Installer LITH](#chapitre-2--installer-lith)
- [Chapitre 3 — Votre premier site en 5 minutes](#chapitre-3--votre-premier-site-en-5-minutes)
- [Chapitre 4 — La ligne de commande](#chapitre-4--la-ligne-de-commande)
- [Chapitre 5 — Le fichier de configuration lith.conf](#chapitre-5--le-fichier-de-configuration-lithconf)
- [Chapitre 6 — Servir un site web](#chapitre-6--servir-un-site-web)
- [Chapitre 7 — Performance](#chapitre-7--performance)
- [Chapitre 8 — Sécurité](#chapitre-8--sécurité)
- [Chapitre 9 — Le mode développement (rechargement automatique)](#chapitre-9--le-mode-développement-rechargement-automatique)
- [Chapitre 10 — Proxy inverse et répartition de charge](#chapitre-10--proxy-inverse-et-répartition-de-charge)
- [Chapitre 11 — Supervision et journaux](#chapitre-11--supervision-et-journaux)
- [Chapitre 12 — Mettre en production](#chapitre-12--mettre-en-production)
- [Chapitre 13 — Comment LITH fonctionne à l'intérieur](#chapitre-13--comment-lith-fonctionne-à-lintérieur)
- [Chapitre 14 — Compiler, tester et contribuer](#chapitre-14--compiler-tester-et-contribuer)
- [Chapitre 15 — Dépannage et questions fréquentes](#chapitre-15--dépannage-et-questions-fréquentes)
- [Chapitre 16 — Feuille de route et choix techniques (dont Rust)](#chapitre-16--feuille-de-route-et-choix-techniques-dont-rust)
- [Chapitre 17 — Glossaire](#chapitre-17--glossaire)
- [Annexe A — Aide-mémoire](#annexe-a--aide-mémoire)
- [Annexe B — Codes de réponse HTTP utilisés par LITH](#annexe-b--codes-de-réponse-http-utilisés-par-lith)

---

## Chapitre 0 — Introduction

### 0.1 LITH, c'est quoi ?

LITH (pour *Light HTTP Server*) est un **serveur web** : un programme qui
attend qu'un navigateur lui demande une page, puis la lui envoie.

Quand vous tapez une adresse dans votre navigateur, quelque part, un serveur web
reçoit cette demande et répond avec le contenu de la page. LITH joue ce rôle,
sur votre propre ordinateur ou sur un serveur en ligne.

Ses particularités :

- **Il est minuscule** : un seul fichier exécutable d'environ 300 Ko, qui
  consomme quelques mégaoctets de mémoire.
- **Il n'a besoin de rien d'autre** : pas de Node.js, pas de Python, pas de
  bibliothèque à installer. Vous le copiez, vous le lancez, il fonctionne.
- **Il marche partout** : Windows, Linux, macOS, Raspberry Pi, Docker.
- **Il est sûr par défaut** : les protections importantes sont actives sans
  rien configurer.
- **Il est écrit en C11**, un langage très rapide et très portable.

### 0.2 À quoi peut-il servir ?

| Besoin | Ce que LITH fait pour vous |
|--------|----------------------------|
| Voir mon site HTML/CSS/JS dans le navigateur pendant que je le code | `lith start --dev` : la page se recharge toute seule à chaque modification |
| Publier un site vitrine, un portfolio, une documentation | Sert les fichiers vite, compressés, avec cache |
| Héberger une application React / Vue / Angular | Mode `SPA_FALLBACK` pour que les routes de l'application fonctionnent |
| Mettre mon API Node.js / Python derrière une seule adresse | Proxy inverse : `/api` va vers votre application |
| Répartir le trafic sur plusieurs copies de mon application | Répartition de charge avec détection des pannes |
| Protéger une page par mot de passe | `AUTH=/admin utilisateur:motdepasse` |
| Surveiller mon serveur | Pages de statistiques, format Prometheus/Grafana |
| Déployer dans le cloud | Image Docker de ≈ 330 Ko |

### 0.3 Ce que LITH ne fait pas (encore)

Il vaut mieux le savoir dès le début :

- **HTTPS** (le cadenas dans le navigateur) est une **option de compilation** :
  l'exécutable standard n'en contient pas (il reste sans aucune dépendance) ;
  compilé avec `make TLS=1`, LITH sert directement `https://`. Autre solution :
  placer LITH derrière un outil qui gère HTTPS (Caddy, nginx, Cloudflare...).
  Les deux méthodes sont expliquées pas à pas au
  [chapitre 12.4](#124-ajouter-https).
- LITH **n'obtient pas** les certificats tout seul : vous les lui fournissez
  (Let's Encrypt avec `certbot`, par exemple), ou vous laissez Caddy s'en charger.
- LITH **n'exécute pas de code serveur** (PHP, Python...) lui-même. En revanche,
  il peut transmettre les requêtes à une application qui le fait (chapitre 10).
- Il parle HTTP/1.0 et HTTP/1.1 (pas encore HTTP/2 ni HTTP/3). C'est largement
  suffisant pour la grande majorité des sites.

### 0.4 Comment lire ce guide

- Si vous débutez : lisez les chapitres **1 à 6 dans l'ordre**. Vous aurez un
  site en ligne en local à la fin du chapitre 3.
- Si vous voulez mettre un site en ligne : chapitres **8 et 12**.
- Si vous êtes développeur et curieux : chapitres **13, 14 et 16**.

**Conventions** :

- Un bloc comme celui-ci est une commande à taper dans un **terminal** :

  ```bash
  lith start
  ```

- Le *terminal* (ou *invite de commandes*) est la fenêtre où l'on tape des
  commandes : **PowerShell** ou **Invite de commandes** sous Windows,
  **Terminal** sous macOS et Linux.
- `<quelque-chose>` signifie « remplacez par votre valeur ».

---

## Chapitre 1 — Les notions de base du web

Ce chapitre explique, sans jargon inutile, ce qui se passe quand vous ouvrez
une page web. Si vous connaissez déjà HTTP, passez au chapitre 2.

### 1.1 Client et serveur

Le web fonctionne comme un restaurant :

- le **client** (votre navigateur : Chrome, Firefox, Edge, Safari) passe une
  **commande** : « je voudrais la page d'accueil » ;
- le **serveur** (LITH) va chercher le plat en cuisine (le fichier sur le
  disque) et le **sert** au client.

```text
   Navigateur (client)                         LITH (serveur)
  ┌──────────────────┐   1. requête            ┌──────────────────┐
  │                  │ ──────────────────────▶ │                  │
  │  http://.../     │   "GET /index.html"     │  lit le fichier   │
  │                  │                         │  public/index.html│
  │                  │ ◀────────────────────── │                  │
  └──────────────────┘   2. réponse            └──────────────────┘
                          "200 OK" + le HTML
```

### 1.2 HTTP : la langue commune

Le navigateur et le serveur se parlent dans un langage appelé **HTTP**
(*HyperText Transfer Protocol*). C'est du texte très simple. Voici une vraie
requête envoyée par un navigateur :

```http
GET /contact.html HTTP/1.1
Host: localhost:8090
User-Agent: Mozilla/5.0 ...
Accept-Encoding: gzip
```

- `GET` est la **méthode** : « donne-moi ». (Il en existe d'autres : `POST`
  pour envoyer un formulaire, `HEAD` pour demander seulement les informations...)
- `/contact.html` est le **chemin** de la ressource demandée.
- `HTTP/1.1` est la version du protocole.
- Les lignes suivantes sont des **en-têtes** : des informations
  supplémentaires (« j'accepte la compression gzip », « je suis Firefox »...).

Et la réponse de LITH :

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 1234

<!DOCTYPE html>
<html>...
```

- `200 OK` est le **code de statut** : ici, « tout va bien ».
- `Content-Type` dit au navigateur ce qu'il reçoit (une page HTML, une image...).
- Après une ligne vide vient le **corps** : le contenu du fichier.

### 1.3 Les codes de statut

Le code à trois chiffres résume le résultat :

| Famille | Signification | Exemples courants |
|---------|---------------|-------------------|
| 2xx | Succès | `200 OK`, `206 Partial Content` (un morceau de vidéo) |
| 3xx | Redirection ou « rien de neuf » | `301` (l'adresse a changé), `304 Not Modified` (« vous avez déjà la bonne version ») |
| 4xx | Erreur du client | `404 Not Found` (n'existe pas), `403 Forbidden` (interdit), `401` (mot de passe requis), `429` (trop de requêtes) |
| 5xx | Erreur du serveur | `500` (problème interne), `502` (l'application derrière le proxy ne répond pas) |

La liste complète utilisée par LITH est en [annexe B](#annexe-b--codes-de-réponse-http-utilisés-par-lith).

### 1.4 URL, adresse, port et localhost

Une **URL** comme `http://localhost:8090/blog/article.html` se décompose ainsi :

| Partie | Exemple | Signification |
|--------|---------|---------------|
| Protocole | `http://` | La langue utilisée |
| Hôte | `localhost` | La machine à joindre. `localhost` = « mon propre ordinateur » |
| Port | `8090` | La « porte » de la machine où LITH écoute |
| Chemin | `/blog/article.html` | Le fichier demandé |

**Le port**, c'est comme le numéro d'appartement dans un immeuble : l'adresse
désigne l'immeuble (la machine), le port désigne la porte (le programme). Un
même ordinateur peut faire tourner plusieurs serveurs, chacun sur son port.
LITH utilise le port **8090** par défaut. Le port **80** est celui du web
classique (quand on ne précise rien dans l'adresse), le **443** celui de HTTPS.

**`localhost`** (ou `127.0.0.1`) désigne toujours votre propre machine. Un site
ouvert sur `http://localhost:8090` n'est visible que depuis votre ordinateur...
sauf si vous donnez votre adresse réseau à d'autres appareils (voir 6.9).

### 1.5 Site statique ou dynamique ?

- Un **site statique** est fait de fichiers prêts à l'emploi (HTML, CSS,
  JavaScript, images). Le serveur les envoie tels quels. C'est le travail
  principal de LITH, et c'est extrêmement rapide.
- Un **site dynamique** fabrique les pages à la demande avec un programme
  (PHP, Node.js, Python...). LITH ne fabrique pas ces pages lui-même, mais il
  peut se placer devant votre application et lui transmettre les requêtes :
  c'est le **proxy inverse** (chapitre 10).

> 💡 Les applications modernes React, Vue, Svelte ou Angular, une fois
> « construites » (`npm run build`), sont des **sites statiques** : LITH peut
> les servir directement.

---

## Chapitre 2 — Installer LITH

Deux possibilités : télécharger un exécutable prêt à l'emploi (le plus simple),
ou le compiler vous-même.

### 2.1 Télécharger un exécutable (recommandé pour débuter)

1. Allez sur la page des **Releases** : <https://github.com/fomadev/lith/releases>
2. Téléchargez l'archive de votre système :
   - Windows : `lith-v1.0.0-windows-x64.zip`
   - Linux : `lith-v1.0.0-linux-x64.tar.gz`
   - macOS : `lith-v1.0.0-macos.tar.gz`
3. Décompressez-la. Vous obtenez :

   ```text
   lith-v1.0.0-.../
   ├── bin/lith(.exe)     ← le serveur
   ├── public/index.html  ← un site d'exemple
   ├── lith.conf          ← la configuration commentée
   ├── README.md, DOCUMENTATION.md, CHANGELOG.md, LICENSE
   ```

4. Vérifiez que tout fonctionne. Ouvrez un terminal **dans ce dossier** et tapez :

   ```bash
   bin/lith version        # Linux / macOS
   bin\lith.exe version    # Windows
   ```

   Vous devez voir `LITH 1.0.0`.

> 🍎 **macOS** : au premier lancement, macOS peut bloquer un programme
> téléchargé. Autorisez-le dans *Réglages Système → Confidentialité et
> sécurité*, ou tapez `xattr -d com.apple.quarantine bin/lith`.

### 2.2 Rendre la commande `lith` disponible partout

Pour taper simplement `lith` depuis n'importe quel dossier, l'exécutable doit
être dans le **PATH** (la liste des dossiers où le système cherche les commandes).

**Windows** :
1. Créez un dossier, par exemple `C:\lith\bin`, et copiez-y `lith.exe`.
2. Menu Démarrer → tapez « variables d'environnement » → *Modifier les
   variables d'environnement pour votre compte*.
3. Sélectionnez `Path` → *Modifier* → *Nouveau* → `C:\lith\bin` → OK.
4. **Fermez et rouvrez** le terminal, puis tapez `lith version`.

**Linux / macOS** :

```bash
sudo cp bin/lith /usr/local/bin/
lith version
```

### 2.3 Compiler LITH soi-même

Compiler, c'est transformer le code source (fichiers `.c`) en programme
exécutable. Il faut deux outils : un **compilateur C** et **make**.

**Linux (Debian, Ubuntu...)** :

```bash
sudo apt install build-essential git
git clone https://github.com/fomadev/lith.git
cd lith
make
sudo make install      # optionnel : copie lith dans /usr/local/bin
```

**macOS** :

```bash
xcode-select --install          # installe le compilateur (clang) et make
git clone https://github.com/fomadev/lith.git
cd lith
make
```

**Windows** : installez un environnement **MinGW-w64**, au choix :
- [w64devkit](https://github.com/skeeto/w64devkit) : une archive à décompresser,
  rien à installer ;
- [MSYS2](https://www.msys2.org/) : puis `pacman -S mingw-w64-x86_64-gcc make`
  dans le terminal « MSYS2 MINGW64 ».

Puis, dans ce terminal :

```bash
git clone https://github.com/fomadev/lith.git
cd lith
make
```

L'exécutable est créé dans `bin/lith` (ou `bin\lith.exe`). Pour vérifier que
tout est correct :

```bash
make test
```

Vous devez voir `168 vérifications, 0 échec(s)` (le nombre peut augmenter
avec les versions).

**Avec HTTPS (facultatif)** : il faut en plus [Rust](https://rustup.rs)
(`cargo`), qui compile le petit module de chiffrement du dossier `tls/`
(basé sur la bibliothèque rustls), puis :

```bash
make TLS=1
make TLS=1 test
```

Sous Windows avec MinGW, ajoutez une fois pour toutes la cible Rust compatible
avec gcc : `rustup target add x86_64-pc-windows-gnu` (avec MSYS2, installer
`mingw-w64-x86_64-rust` suffit). La première compilation télécharge les
dépendances Rust (versions figées par `tls/Cargo.lock`) et prend une à deux
minutes ; les suivantes sont instantanées. L'exécutable passe d'environ 300 Ko
à environ 1,5 Mo. Sans `TLS=1`, rien de tout cela n'intervient.

### 2.4 Avec Docker

Si vous connaissez Docker, aucune installation n'est nécessaire :

```bash
docker build -t lith .
docker run -p 8090:8090 lith
```

Le détail est au [chapitre 12.5](#125-déployer-avec-docker).

---

## Chapitre 3 — Votre premier site en 5 minutes

### 3.1 Créer le projet

Dans un terminal, placez-vous où vous voulez créer votre site (par exemple vos
Documents), puis :

```bash
lith init mon-site
```

LITH crée :

```text
mon-site/
├── lith.conf          ← la configuration (tous les réglages, commentés)
└── public/
    └── index.html     ← votre page d'accueil
```

Le dossier **`public`** est la racine de votre site : tout ce que vous y mettez
devient accessible dans le navigateur.

### 3.2 Démarrer le serveur

```bash
cd mon-site
lith start
```

Vous voyez quelque chose comme :

```text
[2026-10-18 10:00:00] INFO  LITH 1.0.0 prêt : http://localhost:8090  (dossier 'public', 32 threads, cache on, gzip on)
[2026-10-18 10:00:00] INFO  Appuyez sur Ctrl+C pour arrêter.
```

### 3.3 Ouvrir le site

Ouvrez votre navigateur à l'adresse **<http://localhost:8090>**.
La page « Ça marche ! » s'affiche. 🎉

Dans le terminal, une ligne apparaît pour chaque requête :

```text
[2026-10-18 10:00:05] 127.0.0.1 "GET / HTTP/1.1" 200 772 0ms "Mozilla/5.0 ..."
```

On y lit : qui a demandé (`127.0.0.1` = vous), quoi (`GET /`), le résultat
(`200`), la taille envoyée (772 octets) et le temps de traitement.

### 3.4 Modifier le site

1. Ouvrez `public/index.html` dans votre éditeur (VS Code, Notepad++...).
2. Changez le texte, enregistrez.
3. Rechargez la page dans le navigateur (F5) : la modification apparaît.

Ajoutez d'autres fichiers dans `public/` : `public/contact.html` sera
accessible à `http://localhost:8090/contact.html`, `public/images/logo.png` à
`http://localhost:8090/images/logo.png`.

> 💡 Pour ne plus avoir à appuyer sur F5, lancez plutôt `lith start --dev`
> (chapitre 9) : le navigateur se recharge tout seul.

### 3.5 Arrêter le serveur

Dans le terminal, appuyez sur **Ctrl + C**. LITH termine les requêtes en cours
puis s'arrête :

```text
[...] INFO  Arrêt en cours : fin des requêtes en cours...
[...] INFO  LITH est arrêté. À bientôt !
```

### 3.6 Si quelque chose ne va pas

| Message | Cause | Solution |
|---------|-------|----------|
| `Le port 8090 est déjà utilisé` | Un autre programme (ou un autre LITH) utilise ce port | `lith start 8091`, ou arrêtez l'autre programme |
| `Le dossier public 'public' est introuvable` | Vous n'êtes pas dans le bon dossier | `cd mon-site`, ou `lith start -r chemin/vers/le/site` |
| `lith : commande introuvable` | `lith` n'est pas dans le PATH | Voir 2.2, ou utilisez le chemin complet (`./bin/lith`) |
| La page affiche `404 Not Found` | Le fichier n'existe pas dans `public/` | Vérifiez le nom exact (majuscules, extension) |

D'autres cas sont traités au [chapitre 15](#chapitre-15--dépannage-et-questions-fréquentes).

---

## Chapitre 4 — La ligne de commande

Toutes les commandes s'écrivent `lith <commande> [options]`.

### 4.1 Les commandes

| Commande | Rôle |
|----------|------|
| `lith start [PORT]` | Démarre le serveur. Le port est optionnel (sinon : `lith.conf`, sinon 8090) |
| `lith stop` | Arrête un LITH lancé en arrière-plan avec `-d` (Linux/macOS) |
| `lith init [DOSSIER]` | Crée `lith.conf` et `public/index.html` (sans écraser l'existant) |
| `lith check` | Lit la configuration, signale les erreurs et affiche toutes les valeurs utilisées |
| `lith health [PORT]` | Vérifie qu'un LITH répond sur cette machine. Code de sortie 0 = OK, 1 = injoignable |
| `lith version` | Affiche la version (`-v`, `--version` fonctionnent aussi) |
| `lith help` | Affiche l'aide (`-h`, `--help`) |

### 4.2 Les options

| Option | Effet | Exemple |
|--------|-------|---------|
| `-c`, `--config FICHIER` | Utilise un autre fichier que `lith.conf` | `lith start -c prod.conf` |
| `-p`, `--port PORT` | Port d'écoute | `lith start -p 3000` |
| `-H`, `--host ADRESSE` | Adresse d'écoute | `lith start -H 127.0.0.1` |
| `-r`, `--root DOSSIER` | Dossier à servir | `lith start -r ./dist` |
| `-d`, `--daemon` | Arrière-plan (Linux/macOS) | `lith start -d` |
| `--dev` | Mode développement : rechargement automatique | `lith start --dev` |
| `-q`, `--quiet` | N'affiche pas une ligne par requête | `lith start -q` |

**Ordre de priorité** des réglages : valeur par défaut → `lith.conf` → options
de la ligne de commande. Une option tapée dans le terminal gagne toujours.

### 4.3 Exemples courants

```bash
# Servir le dossier de build d'une application React sur le port 3000
lith start -r ./build -p 3000

# Développer avec rechargement automatique
lith start --dev

# Vérifier une configuration de production avant de l'utiliser
lith check -c /etc/lith/prod.conf

# Lancer en arrière-plan sur un serveur Linux, puis l'arrêter
lith start -d
lith stop

# Dans un script : attendre que le serveur soit prêt
until lith health; do sleep 1; done
```

### 4.4 Codes de sortie

| Code | Signification |
|------|---------------|
| 0 | Succès (ou arrêt normal) |
| 1 | Erreur : configuration invalide, port occupé, serveur injoignable... |
| 2 | Commande ou option mal écrite |

---

## Chapitre 5 — Le fichier de configuration lith.conf

### 5.1 Le format

`lith.conf` est un simple fichier texte :

```ini
# Ceci est un commentaire (ligne ignorée)
PORT=8090
PUBLIC_DIR=public
```

Règles :
- un réglage par ligne, au format `CLE=VALEUR` ;
- les lignes commençant par `#` (ou `;`) sont des commentaires ;
- les espaces autour du `=` sont ignorés ; les guillemets autour de la valeur
  sont facultatifs ;
- les clés ne sont pas sensibles à la casse (`port` = `PORT`) ;
- **booléens** : `1`/`0`, `true`/`false`, `on`/`off`, `yes`/`no`, `oui`/`non` ;
- **tailles** : un nombre d'octets, ou avec une unité : `512K`, `10M`, `1G` ;
- `AUTH` et `PROXY` peuvent être répétées sur plusieurs lignes.

**Toutes les clés sont facultatives.** Sans `lith.conf`, LITH démarre avec les
valeurs par défaut. Une clé inconnue ou une valeur invalide est signalée avec
le numéro de ligne, et LITH refuse de démarrer : vous ne restez jamais avec une
configuration à moitié appliquée.

> ✅ Après toute modification, lancez `lith check` : il affiche les erreurs et
> la configuration réellement utilisée.

### 5.2 Réseau

| Clé | Défaut | Description |
|-----|--------|-------------|
| `HOST` | `0.0.0.0` | Adresse d'écoute. `0.0.0.0` = toutes les interfaces IPv4 (accessible depuis le réseau local). `127.0.0.1` = uniquement depuis cette machine. `::` = IPv4 et IPv6 |
| `PORT` | `8090` | Port d'écoute (1 à 65535). Les ports < 1024 demandent des droits administrateur sous Linux |
| `THREADS` | `0` (auto) | Nombre de threads de travail. Auto = 8 × nombre de cœurs, entre 16 et 128 |
| `MAX_PENDING` | `1024` | Connexions pouvant attendre un thread libre. Au-delà : réponse `503` immédiate |
| `KEEPALIVE_TIMEOUT` | `5` | Secondes pendant lesquelles une connexion inactive reste ouverte pour la requête suivante. `0` désactive le keep-alive |
| `KEEPALIVE_MAX` | `100` | Nombre maximum de requêtes par connexion |
| `REQUEST_TIMEOUT` | `10` | Secondes accordées pour envoyer les en-têtes d'une requête (et entre deux morceaux du corps). Protège contre les attaques « slowloris » |
| `MAX_BODY_SIZE` | `10M` | Taille maximum du corps d'une requête (formulaire, envoi de fichier vers le proxy). Au-delà : `413` |

### 5.3 Fichiers

| Clé | Défaut | Description |
|-----|--------|-------------|
| `PUBLIC_DIR` | `public` | Dossier contenant le site. Chemin relatif (au dossier où l'on lance LITH) ou absolu |
| `INDEX_FILE` | `index.html` | Fichier servi quand on demande un dossier (`/` ou `/blog/`) |
| `AUTO_INDEX` | `0` | `1` = afficher la liste des fichiers d'un dossier qui n'a pas d'`index.html` |
| `SPA_FALLBACK` | `0` | `1` = pour les applications monopage : une adresse inconnue **sans extension** (`/profil/42`) renvoie `index.html` |
| `HIDE_DOTFILES` | `1` | `1` = les fichiers et dossiers commençant par un point (`.env`, `.git/`) sont introuvables (`404`). Seul `/.well-known/` reste accessible |
| `CACHE_CONTROL` | `no-cache` | Valeur de l'en-tête `Cache-Control` des fichiers (voir 7.4). Vide = non envoyé |

### 5.4 Performance

| Clé | Défaut | Description |
|-----|--------|-------------|
| `USE_CACHE` | `1` | Garder les fichiers en mémoire vive. Un fichier modifié sur le disque est rechargé automatiquement |
| `CACHE_MAX_SIZE` | `64M` | Mémoire maximum utilisée par le cache. Les fichiers les moins demandés sont retirés en premier |
| `CACHE_FILE_MAX` | `2M` | Taille maximum d'un fichier pour entrer dans le cache. Les plus gros sont lus directement sur le disque |
| `GZIP` | `1` | Compresser les fichiers texte (HTML, CSS, JS, JSON, SVG...) pour les navigateurs qui l'acceptent |

### 5.5 Sécurité

| Clé | Défaut | Description |
|-----|--------|-------------|
| `SECURITY_HEADERS` | `1` | Ajoute `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN` et `Referrer-Policy` (voir 8.2) |
| `CSP` | *(vide)* | Valeur de l'en-tête `Content-Security-Policy`. Ex. : `default-src 'self'` |
| `CORS_ORIGIN` | *(vide)* | Autorise les appels JavaScript depuis un autre site. `*` = tout le monde, ou une origine précise : `https://monsite.fr` |
| `RATE_LIMIT` | `0` | Nombre de requêtes par seconde autorisées **par adresse IP**. `0` = pas de limite. Au-delà : `429` |
| `RATE_BURST` | `2 × RATE_LIMIT` | Réserve de requêtes autorisées d'un coup (rafale). Si absente ou inférieure à `RATE_LIMIT`, vaut `2 × RATE_LIMIT` |
| `AUTH` | — | Protège un chemin par mot de passe : `AUTH=/admin utilisateur:motdepasse`. Répétable (16 max) |

### 5.6 Proxy inverse

| Clé | Défaut | Description |
|-----|--------|-------------|
| `PROXY` | — | `PROXY=/prefixe http://hote:port[, http://hote2:port2...]`. Répétable (16 routes, 8 backends par route) |
| `PROXY_TIMEOUT` | `30` | Secondes d'attente maximum de la réponse d'un backend. Au-delà : `504` |
| `HEALTH_CHECK_INTERVAL` | `5` | Secondes entre deux vérifications des backends. `0` = désactivé |

### 5.7 Développement et supervision

| Clé | Défaut | Description |
|-----|--------|-------------|
| `LIVE_RELOAD` | `0` | Recharge le navigateur à chaque modification de fichier (activé par `--dev`) |
| `STATUS` | `1` | Active `/__lith/status` et `/__lith/metrics` |
| `STATUS_AUTH` | *(vide)* | `utilisateur:motdepasse`. Sans cette clé, les statistiques ne sont visibles que depuis la machine elle-même |

### 5.8 Journaux et processus

| Clé | Défaut | Description |
|-----|--------|-------------|
| `LOG_LEVEL` | `info` | Niveau de détail : `debug`, `info`, `warn`, `error` |
| `LOG_FILE` | *(vide)* | Fichier où écrire les journaux. Vide = dans le terminal (en mode démon : `lith.log`) |
| `ACCESS_LOG` | `1` | Une ligne par requête (désactivable avec `-q`) |
| `PID_FILE` | `lith.pid` | Fichier contenant le numéro du processus en mode démon (utilisé par `lith stop`) |

### 5.9 Exemples de configurations complètes

**Développement local**

```ini
HOST=127.0.0.1
PORT=8090
LIVE_RELOAD=1
CACHE_CONTROL=no-store
```

**Application React/Vue en production**

```ini
PORT=8090
PUBLIC_DIR=/srv/mon-app/dist
SPA_FALLBACK=1
GZIP=1
USE_CACHE=1
CACHE_MAX_SIZE=128M
RATE_LIMIT=50
CSP=default-src 'self'; img-src 'self' data:
```

**Site + API Node.js**

```ini
PORT=8090
PUBLIC_DIR=public
PROXY=/api http://127.0.0.1:3000
AUTH=/admin admin:un-mot-de-passe-long
```

**Site public en HTTPS direct** (exécutable compilé avec `make TLS=1`, voir 5.10)

```ini
PORT=80
TLS_PORT=443
TLS_CERT=/etc/letsencrypt/live/monsite.fr/fullchain.pem
TLS_KEY=/etc/letsencrypt/live/monsite.fr/privkey.pem
TLS_REDIRECT=1
HSTS=31536000
```

### 5.10 HTTPS (`make TLS=1`)

Ces clés ne servent qu'avec un exécutable compilé avec `make TLS=1`
(chapitre 2.3). Avec l'exécutable standard, `TLS_PORT` est **refusé** avec un
message clair : LITH ne démarre jamais en clair alors que vous attendiez du
chiffrement.

| Clé | Défaut | Description |
|-----|--------|-------------|
| `TLS_PORT` | `0` | Port HTTPS (`443` en production, `8443` pour essayer). `0` = HTTPS désactivé. Le port HTTP (`PORT`) reste ouvert en parallèle et doit être différent |
| `TLS_CERT` | *(vide)* | Certificat au format PEM, **chaîne complète** (votre certificat suivi des certificats intermédiaires : `fullchain.pem` chez Let's Encrypt) |
| `TLS_KEY` | *(vide)* | Clé privée au format PEM (PKCS#8, RSA ou EC : `privkey.pem` chez Let's Encrypt). Protégez ce fichier (`chmod 600`) |
| `TLS_REDIRECT` | `0` | `1` = le port HTTP répond par une redirection vers `https://` (`301` pour `GET`/`HEAD`, `308` pour les autres méthodes, qui conserve le corps). Restent en clair : `/__lith/health` (sondes, `lith health`) et `/.well-known/acme-challenge/` (validation Let's Encrypt) |
| `HSTS` | `0` | Durée en secondes de l'en-tête `Strict-Transport-Security`, envoyé uniquement en HTTPS : le navigateur n'utilisera plus que HTTPS pour ce site pendant cette durée. Commencez petit (`300`), puis `31536000` (un an) une fois sûr de vous |

Les chemins relatifs sont relatifs au dossier où l'on lance LITH (ils sont
convertis en chemins absolus en mode démon). `lith check` charge réellement le
certificat et la clé et signale tout problème (fichier absent, PEM illisible,
clé qui ne correspond pas au certificat).

Ce que fait LITH en HTTPS : TLS 1.3 et TLS 1.2 uniquement, suites de
chiffrement modernes, annonce de HTTP/1.1 par ALPN, reprise de session. Tout le
reste fonctionne à l'identique : fichiers, gzip, cache, `Range`, keep-alive,
proxy (le backend reçoit `X-Forwarded-Proto: https`), live reload en `wss://`,
statistiques. Les protections contre les clients lents s'appliquent aussi à la
poignée de main TLS (`REQUEST_TIMEOUT`).

Pour essayer en local avec un certificat auto-signé (le navigateur affichera un
avertissement, c'est normal) :

```bash
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -keyout key.pem -out cert.pem \
  -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
printf 'TLS_PORT=8443\nTLS_CERT=cert.pem\nTLS_KEY=key.pem\n' >> lith.conf
lith start          # puis https://localhost:8443
```

> 💡 Sous Git Bash (Windows), préfixez la commande `openssl` par
> `MSYS_NO_PATHCONV=1` pour que `/CN=localhost` ne soit pas pris pour un chemin.

---

## Chapitre 6 — Servir un site web

### 6.1 Du chemin de l'URL au fichier

LITH ajoute le chemin de l'URL au dossier `PUBLIC_DIR` :

| URL demandée | Fichier envoyé |
|--------------|----------------|
| `/` | `public/index.html` |
| `/contact.html` | `public/contact.html` |
| `/css/style.css` | `public/css/style.css` |
| `/blog/` | `public/blog/index.html` |
| `/blog` | redirection `301` vers `/blog/` (pour que les liens relatifs de la page fonctionnent) |

Les caractères spéciaux sont décodés : `/mon%20fichier.html` donne
`public/mon fichier.html`, et les noms accentués (`café.html`) fonctionnent sur
tous les systèmes, Windows compris.

### 6.2 Les types de fichiers (MIME)

Le navigateur doit savoir ce qu'il reçoit. LITH l'indique dans l'en-tête
`Content-Type` d'après l'extension : `.html`, `.css`, `.js`, `.mjs`, `.json`,
`.svg`, `.png`, `.jpg`, `.webp`, `.avif`, `.gif`, `.ico`, `.woff2`, `.ttf`,
`.mp4`, `.webm`, `.mp3`, `.ogg`, `.pdf`, `.wasm`, `.zip`, `.webmanifest`,
`.xml`, `.txt`, `.md`, `.csv`... (40 types). Un fichier d'extension inconnue
est envoyé comme `application/octet-stream` : le navigateur proposera de le
télécharger.

### 6.3 Pages d'erreur personnalisées

Par défaut, LITH affiche une page d'erreur sobre. Pour utiliser la vôtre, créez
dans `public/` un fichier nommé d'après le code : `404.html`, `403.html`,
`500.html`... LITH l'enverra automatiquement, avec le bon code de statut.

### 6.4 Applications monopage (React, Vue, Angular, Svelte...)

Ces applications gèrent elles-mêmes leurs adresses (`/profil/42`,
`/parametres`) dans le navigateur. Mais si l'utilisateur recharge la page ou
ouvre un lien directement, le serveur reçoit `/profil/42`, qui n'existe pas sur
le disque.

Avec `SPA_FALLBACK=1`, LITH renvoie `index.html` pour toute adresse inconnue
**sans extension** ; l'application prend ensuite le relais. Les fichiers
manquants avec extension (`/logo.png`) répondent toujours `404`, ce qui évite
de masquer de vraies erreurs.

```ini
PUBLIC_DIR=dist
SPA_FALLBACK=1
```

### 6.5 Lister le contenu d'un dossier

Avec `AUTO_INDEX=1`, un dossier sans `index.html` affiche la liste de ses
fichiers (nom, taille, date), triée, dossiers en premier. Pratique pour
partager des fichiers sur un réseau local. Les fichiers cachés n'y apparaissent
pas tant que `HIDE_DOTFILES=1`.

### 6.6 Les fichiers cachés

Les fichiers commençant par un point contiennent souvent des secrets : `.env`
(mots de passe), `.git/` (tout l'historique du code), `.htpasswd`... Avec
`HIDE_DOTFILES=1` (par défaut), LITH répond `404` comme s'ils n'existaient pas.
Exception : `/.well-known/`, utilisé par des standards du web (certificats,
`security.txt`...).

### 6.7 Les méthodes acceptées

Pour les fichiers statiques, LITH accepte `GET`, `HEAD` et `OPTIONS`. Une autre
méthode (`POST`, `PUT`, `DELETE`...) répond `405 Method Not Allowed`. Ces
méthodes sont en revanche transmises telles quelles aux applications derrière
le proxy (chapitre 10).

### 6.8 Les adresses réservées `/__lith/`

Les chemins commençant par `/__lith/` sont réservés à LITH :

| Adresse | Rôle |
|---------|------|
| `/__lith/health` | Répond `OK` : sert aux sondes de disponibilité (Docker, Kubernetes) |
| `/__lith/status` | Statistiques en JSON (chapitre 11) |
| `/__lith/metrics` | Statistiques au format Prometheus (chapitre 11) |
| `/__lith/livereload` | Canal WebSocket du rechargement automatique (chapitre 9) |
| `/__lith/echo` | Diagnostic : en `POST`, renvoie ce que LITH a reçu (taille, type) |

### 6.9 Voir le site depuis un téléphone ou un autre ordinateur

1. Laissez `HOST=0.0.0.0` (valeur par défaut).
2. Trouvez l'adresse de votre ordinateur sur le réseau : `ipconfig` (Windows),
   `ip addr` (Linux), *Réglages → Réseau* (macOS). Par exemple `192.168.1.20`.
3. Sur le téléphone, connecté au **même Wi-Fi**, ouvrez `http://192.168.1.20:8090`.
4. Si rien ne s'affiche, autorisez LITH dans le **pare-feu** (Windows le
   demande généralement au premier lancement).

---

## Chapitre 7 — Performance

LITH est rapide sans réglage. Ce chapitre explique pourquoi, et comment
l'ajuster.

### 7.1 Le cache mémoire

Lire un fichier sur le disque prend du temps ; le lire en mémoire vive est
quasi instantané. Avec `USE_CACHE=1`, la première fois qu'un fichier est
demandé, LITH le garde en mémoire. Les fois suivantes, il l'envoie directement
depuis la RAM.

- **Toujours à jour** : à chaque demande, LITH vérifie la date de modification
  du fichier. S'il a changé, il est rechargé. Vous pouvez modifier votre site
  sans redémarrer le serveur.
- **Borné** : la mémoire totale ne dépasse pas `CACHE_MAX_SIZE`. Quand c'est
  plein, les fichiers les moins récemment demandés sont retirés (algorithme
  *LRU*).
- Les fichiers plus gros que `CACHE_FILE_MAX` (vidéos, archives) ne passent pas
  par le cache : ils sont envoyés par morceaux depuis le disque, sans jamais
  être chargés entièrement en mémoire. Sous Linux, LITH utilise `sendfile()`,
  qui fait passer les données du disque au réseau sans même les copier dans le
  programme.

### 7.2 La compression gzip

Les fichiers texte se compressent très bien : un fichier HTML ou JavaScript
perd souvent **60 à 75 %** de sa taille. Avec `GZIP=1`, si le navigateur
l'accepte (en-tête `Accept-Encoding: gzip`, envoyé par tous les navigateurs
modernes), LITH envoie la version compressée.

- La version compressée est calculée **une seule fois** puis gardée en cache.
- Les images, vidéos et archives (déjà compressées) ne sont pas recompressées.
- Les fichiers de moins de 256 octets non plus (aucun gain).
- La compression est **intégrée à LITH** (aucune bibliothèque externe).

### 7.3 « Vous avez déjà la bonne version » : ETag et 304

Chaque fichier envoyé porte une **étiquette** (`ETag`) et sa date de
modification (`Last-Modified`). Quand le navigateur redemande le fichier, il
présente son étiquette ; si le fichier n'a pas changé, LITH répond simplement
`304 Not Modified`, sans renvoyer le contenu. Résultat : des pages qui
s'affichent instantanément et une bande passante économisée.

### 7.4 Régler le cache du navigateur (`CACHE_CONTROL`)

L'en-tête `Cache-Control` dit au navigateur combien de temps garder un fichier
sans redemander :

| Valeur | Effet | Quand l'utiliser |
|--------|-------|------------------|
| `no-cache` (défaut) | Le navigateur garde le fichier mais vérifie à chaque fois (réponse `304` très légère) | Le bon choix par défaut : toujours à jour, et rapide |
| `no-store` | Rien n'est gardé | Développement (activé par `--dev`) |
| `public, max-age=3600` | Gardé 1 heure sans aucune vérification | Sites qui changent rarement |
| `public, max-age=31536000, immutable` | Gardé 1 an | Fichiers dont le nom change à chaque version (`app.3f2a1c.js`, produits par Vite/Webpack) |

### 7.5 Les requêtes partielles (vidéo, reprise de téléchargement)

Quand vous avancez dans une vidéo, le navigateur ne demande qu'un morceau du
fichier : `Range: bytes=1000000-2000000`. LITH répond `206 Partial Content`
avec exactement ce morceau. Cela permet la lecture fluide des vidéos et audios,
et la reprise des téléchargements interrompus.

### 7.6 Keep-alive et threads

- **Keep-alive** : ouvrir une connexion réseau coûte du temps. LITH garde la
  connexion ouverte quelques secondes (`KEEPALIVE_TIMEOUT`) pour que le
  navigateur y fasse passer les requêtes suivantes (CSS, images...).
- **Threads** : LITH traite plusieurs visiteurs en même temps grâce à un groupe
  de « travailleurs » (threads) créés au démarrage. Le nombre automatique
  convient presque toujours. Si beaucoup de visiteurs restent connectés
  longtemps, augmentez `THREADS`.
- **Sous forte charge**, quand des visiteurs attendent un thread libre, LITH
  ferme les connexions inactives après chaque réponse pour servir tout le monde
  équitablement.

### 7.7 Quelques ordres de grandeur

Sur un ordinateur portable ordinaire, LITH 1.0.0 sert plusieurs milliers de
requêtes par seconde avec moins de 10 Mo de mémoire. Les tests automatiques
envoient des milliers de requêtes simultanées sans erreur ni fuite de
ressources. Le facteur limitant est généralement le réseau, pas LITH.

---

## Chapitre 8 — Sécurité

### 8.1 Les protections automatiques

Sans aucune configuration, LITH :

| Protection | Contre quoi |
|------------|-------------|
| Refuse `../` dans les chemins, même encodé (`%2e%2e`) ou avec `\` | Lire des fichiers hors du site (`/../../etc/passwd`) → `403` |
| Refuse l'octet nul `%00` et les pièges propres à Windows (`..` suivi d'espaces ou de points, `:`) | Les variantes de la même attaque |
| Cache les fichiers `.env`, `.git/`... | La fuite de mots de passe ou de code source |
| Limite les en-têtes à 16 Ko, l'URL à 2048 caractères, le corps à `MAX_BODY_SIZE` | L'épuisement de la mémoire |
| Coupe les requêtes trop lentes (`REQUEST_TIMEOUT`) | L'attaque « slowloris » (occuper le serveur en envoyant une requête octet par octet) |
| Rejette les requêtes ambiguës (deux `Content-Length`, `Content-Length` + `Transfer-Encoding`...) | Le « request smuggling » (tromper un proxy placé devant) |
| Refuse les connexions au-delà de `MAX_PENDING` avec `503` | La saturation |
| Ajoute des en-têtes de sécurité | Voir 8.2 |

### 8.2 Les en-têtes de sécurité

Avec `SECURITY_HEADERS=1` (par défaut), chaque réponse contient :

- `X-Content-Type-Options: nosniff` : le navigateur ne « devine » pas le type
  d'un fichier (empêche d'exécuter un fichier texte comme du JavaScript) ;
- `X-Frame-Options: SAMEORIGIN` : votre site ne peut pas être affiché dans un
  cadre (`<iframe>`) sur un autre site, ce qui bloque le *clickjacking* ;
- `Referrer-Policy: strict-origin-when-cross-origin` : les adresses complètes
  de vos pages ne sont pas transmises aux autres sites.

### 8.3 Content-Security-Policy (CSP)

La CSP indique au navigateur d'où le site a le droit de charger des scripts,
styles et images. C'est la meilleure protection contre les failles XSS
(injection de JavaScript). Elle n'est pas activée par défaut car une CSP mal
réglée peut casser un site. Pour commencer :

```ini
CSP=default-src 'self'
```

Ajoutez ensuite les sources dont vous avez besoin, par exemple
`CSP=default-src 'self'; img-src 'self' data:; font-src https://fonts.gstatic.com`.
Ouvrez la console du navigateur (F12) : chaque blocage y est signalé.

> ⚠️ Le rechargement automatique (`--dev`) injecte un petit script dans les
> pages : n'activez pas une CSP stricte en même temps que `--dev`.

### 8.4 CORS : autoriser d'autres sites à appeler le vôtre

Par sécurité, un navigateur interdit au JavaScript de `site-a.fr` de lire les
réponses de `site-b.fr`. Si vous voulez l'autoriser (par exemple pour une API
publique ou des fichiers JSON) :

```ini
CORS_ORIGIN=*                     # tout le monde
CORS_ORIGIN=https://site-a.fr     # un seul site
```

LITH ajoute alors `Access-Control-Allow-Origin` et répond aux requêtes de
vérification préalable (`OPTIONS`, dites *preflight*).

### 8.5 Limiter le nombre de requêtes par visiteur

```ini
RATE_LIMIT=20      # 20 requêtes par seconde et par adresse IP
RATE_BURST=40      # avec une réserve de 40 d'un coup
```

C'est l'algorithme du **seau à jetons** : chaque visiteur a un seau de
`RATE_BURST` jetons, qui se remplit de `RATE_LIMIT` jetons par seconde. Chaque
requête consomme un jeton ; seau vide = réponse `429 Too Many Requests` avec
`Retry-After: 1`. Une page normale (HTML + CSS + images) passe sans souci ; un
robot qui envoie des centaines de requêtes est freiné.

> ⚠️ Si LITH est derrière un autre proxy (Caddy, nginx), toutes les requêtes
> semblent venir de `127.0.0.1` : faites alors la limitation dans ce proxy.

### 8.6 Protéger une zone par mot de passe

```ini
AUTH=/admin admin:un-mot-de-passe-long
AUTH=/prive/rapports equipe:autre-mot-de-passe
```

Le navigateur affiche une fenêtre de connexion pour `/admin` et tout ce qui est
dessous (`/admin/stats.html`...), mais pas pour `/administration` (le préfixe
doit être suivi de `/` ou de la fin du chemin). La règle la plus précise
l'emporte. `AUTH` protège aussi les routes du proxy.

> ⚠️ **Important** : l'authentification Basic envoie le mot de passe à chaque
> requête, simplement encodé (pas chiffré). Sur Internet, utilisez-la
> **uniquement derrière HTTPS** (chapitre 12.4). Le mot de passe étant écrit en
> clair dans `lith.conf`, protégez ce fichier (`chmod 600 lith.conf` sous Linux).

### 8.7 HTTPS

HTTPS chiffre les échanges (le cadenas du navigateur) et prouve au visiteur
qu'il parle bien à votre site. Il est indispensable pour un site public, et
obligatoire dès qu'il y a un mot de passe (`AUTH`) ou un formulaire. Deux
possibilités :

- **HTTPS intégré** : compilez LITH avec `make TLS=1`, puis indiquez
  `TLS_PORT`, `TLS_CERT` et `TLS_KEY` (chapitre 5.10). Le chiffrement est
  confié à un petit module écrit en Rust avec la bibliothèque **rustls**, sûre
  en mémoire et auditée (chapitre 16.4) ; le reste de LITH ne change pas.
- **HTTPS devant LITH** : un outil comme **Caddy** obtient et renouvelle les
  certificats tout seul, et transmet les requêtes à LITH en local.

Le pas-à-pas des deux méthodes est au [chapitre 12.4](#124-ajouter-https).

Une fois HTTPS en place :

- `TLS_REDIRECT=1` renvoie automatiquement les visiteurs de `http://` vers
  `https://` ;
- `HSTS` demande au navigateur de ne plus jamais revenir en clair. Attention :
  le navigateur s'en souvient pendant toute la durée indiquée, même si vous
  retirez HTTPS ensuite. Testez d'abord avec une petite valeur (`HSTS=300`).

LITH n'accepte que TLS 1.2 et TLS 1.3 : les navigateurs de plus de dix ans environ
(Internet Explorer 10 et antérieurs, Android 4.x) ne pourront pas se connecter.

### 8.8 Liste de contrôle avant de publier un site

- [ ] HTTPS en place : `make TLS=1` + `TLS_PORT`, ou un proxy devant LITH (Caddy, nginx, Cloudflare...).
- [ ] `TLS_REDIRECT=1`, puis `HSTS` une fois HTTPS validé ; clé privée lisible par LITH seulement.
- [ ] `HIDE_DOTFILES=1` et aucun fichier secret dans `public/`.
- [ ] Mots de passe `AUTH` / `STATUS_AUTH` longs et uniques, `lith.conf` protégé.
- [ ] `RATE_LIMIT` réglé (dans LITH ou dans le proxy de devant).
- [ ] Une `CSP` testée.
- [ ] LITH lancé avec un utilisateur sans privilèges (pas `root`).
- [ ] `lith check` sans erreur.

---

## Chapitre 9 — Le mode développement (rechargement automatique)

### 9.1 L'utiliser

```bash
lith start --dev
```

Ouvrez votre site, modifiez un fichier dans `public/` et enregistrez : le
navigateur se recharge **tout seul** en environ une seconde. Fini les F5 !

`--dev` active `LIVE_RELOAD=1` et `CACHE_CONTROL=no-store` (le navigateur ne
garde rien en cache, vous voyez toujours la dernière version). Vous pouvez
aussi mettre `LIVE_RELOAD=1` dans `lith.conf`.

### 9.2 Comment ça marche

1. Quand LITH envoie une page HTML, il ajoute juste avant `</body>` un petit
   script (≈ 300 caractères).
2. Ce script ouvre un **WebSocket** vers `/__lith/livereload` : une connexion
   qui reste ouverte et permet au serveur d'envoyer un message au navigateur
   à tout moment.
3. De son côté, LITH examine le dossier `public/` chaque seconde. Dès qu'un
   fichier est ajouté, modifié ou supprimé, il envoie le message `reload` à
   tous les navigateurs connectés.
4. Le script reçoit `reload` et recharge la page. Si LITH redémarre, le script
   se reconnecte tout seul.

Le fichier HTML sur votre disque n'est jamais modifié : le script n'est ajouté
que dans la réponse envoyée.

### 9.3 Limites

- Réservé au développement : ne l'activez pas en production.
- Jusqu'à 64 onglets connectés en même temps.
- Les dossiers cachés et `node_modules/` ne sont pas surveillés.
- En mode dev, les pages HTML ne sont ni compressées ni mises en cache (les
  autres fichiers, si).

---

## Chapitre 10 — Proxy inverse et répartition de charge

### 10.1 Le principe

Votre site a une partie statique (HTML, CSS) et une API écrite en Node.js,
Python, Go, PHP... qui tourne sur le port 3000. Plutôt que d'exposer deux
adresses, LITH sert les fichiers et **transmet** à l'application tout ce qui
commence par `/api` :

```text
                           ┌───────────────────────────┐
  navigateur ──▶ LITH :8090 │ /api/...  ──▶ app :3000   │
                           │ le reste  ──▶ public/     │
                           └───────────────────────────┘
```

```ini
PROXY=/api http://127.0.0.1:3000
```

Le chemin est transmis **tel quel** : `/api/users?id=4` arrive à l'application
sous la forme `/api/users?id=4`. Toutes les méthodes (`GET`, `POST`, `PUT`,
`DELETE`...) et le corps des requêtes sont transmis.

### 10.2 Exemple complet avec Node.js

`app.js` :

```js
const http = require('http');
http.createServer((req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.end(JSON.stringify({ message: 'Bonjour depuis Node', chemin: req.url }));
}).listen(3000, '127.0.0.1');
```

`lith.conf` :

```ini
PORT=8090
PROXY=/api http://127.0.0.1:3000
```

```bash
node app.js          # dans un premier terminal
lith start           # dans un second
```

`http://localhost:8090/` affiche votre site, et
`http://localhost:8090/api/test` répond
`{"message":"Bonjour depuis Node","chemin":"/api/test"}`.

### 10.3 Plusieurs copies de l'application : la répartition de charge

Pour tenir plus de trafic ou continuer à fonctionner si une copie tombe en
panne, lancez plusieurs copies de l'application et listez-les :

```ini
PROXY=/api http://127.0.0.1:3000, http://127.0.0.1:3001, http://10.0.0.5:3000
```

- Les requêtes sont distribuées **à tour de rôle** (*round-robin*).
- Si une copie ne répond pas, LITH la marque **hors service**, passe à la
  suivante, et le note dans le journal :
  `WARN  Proxy : backend 127.0.0.1:3001 hors service`.
- Toutes les `HEALTH_CHECK_INTERVAL` secondes, LITH reteste les copies ; dès
  qu'une copie répond de nouveau, elle est réintégrée.
- Si aucune copie ne répond : `502 Bad Gateway`. Si une copie met plus de
  `PROXY_TIMEOUT` secondes à répondre : `504 Gateway Timeout`.

L'état de chaque backend est visible dans `/__lith/status`.

### 10.4 Ce que reçoit l'application

LITH ajoute des en-têtes standard pour que l'application connaisse le vrai
visiteur :

| En-tête | Contenu |
|---------|---------|
| `X-Forwarded-For` | Adresse IP du visiteur (ajoutée à la liste existante) |
| `X-Real-IP` | Adresse IP du visiteur |
| `X-Forwarded-Host` | Le `Host` demandé par le visiteur |
| `X-Forwarded-Proto` | `https` si le visiteur est arrivé sur `TLS_PORT`, sinon `http` |

L'en-tête `Host` d'origine est conservé. Les en-têtes `X-Forwarded-*` et
`X-Real-IP` envoyés par le visiteur lui-même sont remplacés (sauf
`X-Forwarded-For`, complété) : l'application peut donc se fier à
`X-Forwarded-Proto` pour savoir si la connexion était chiffrée (cookies
`Secure`, liens absolus...). La liaison LITH → backend reste en HTTP clair :
gardez vos backends sur `127.0.0.1` ou sur un réseau privé.

### 10.5 Plusieurs routes

```ini
PROXY=/api http://127.0.0.1:3000
PROXY=/auth http://127.0.0.1:4000
PROXY=/api/v2 http://127.0.0.1:5000
```

La route la plus précise gagne : `/api/v2/users` va au port 5000, `/api/users`
au port 3000. Une route `PROXY=/ http://...` transmet **tout** (LITH devient
alors un pur proxy devant votre application).

### 10.6 Limites de la 1.0.0

- Backends en `http://` uniquement (pas `https://`).
- Une nouvelle connexion vers le backend par requête, et la connexion avec le
  visiteur est fermée après une réponse proxifiée (simple et robuste, un peu
  moins rapide que la réutilisation).
- Les WebSockets ne sont pas transmis aux backends (`501`).
- Les corps de requête envoyés « par morceaux » (`Transfer-Encoding: chunked`)
  sont refusés (`411`) : le client doit indiquer `Content-Length` (c'est le cas
  des formulaires et de la plupart des clients HTTP).

---

## Chapitre 11 — Supervision et journaux

### 11.1 Le journal du terminal

Deux sortes de lignes :

```text
[2026-10-18 10:00:00] INFO  LITH 1.0.0 prêt : http://localhost:8090 ...
[2026-10-18 10:00:05] 127.0.0.1 "GET /index.html HTTP/1.1" 200 772 0ms "Mozilla/5.0 ..."
```

- Les **messages** du serveur, avec leur niveau (`DEBUG`, `INFO`, `WARN`, `ERROR`).
- Le **journal d'accès** : une ligne par requête :
  `IP "MÉTHODE CHEMIN VERSION" CODE OCTETS DURÉE "NAVIGATEUR"`.

Réglages : `LOG_LEVEL=debug` pour tout voir (utile pour un diagnostic),
`LOG_FILE=lith.log` pour écrire dans un fichier, `ACCESS_LOG=0` ou `-q` pour
masquer le journal d'accès.

### 11.2 `/__lith/health` : « es-tu vivant ? »

Répond `200 OK` avec le texte `OK`. Accessible à tous. Utilisé par Docker,
Kubernetes, les répartiteurs de charge et la commande `lith health`.

### 11.3 `/__lith/status` : le tableau de bord JSON

```json
{
  "server": "LITH",
  "version": "1.0.0",
  "started_at": "2026-10-18T08:00:00Z",
  "uptime_seconds": 3600,
  "pid": 4242,
  "worker_threads": 32,
  "memory_rss_bytes": 7147520,
  "connections": { "active": 3, "total": 5028 },
  "requests": { "total": 5011, "1xx": 0, "2xx": 4990, "3xx": 2, "4xx": 19, "5xx": 0,
                "rate_limited": 0, "avg_duration_ms": 0.15 },
  "bytes_sent": 9482635,
  "cache": { "enabled": true, "entries": 39, "bytes": 2364648, "hits": 4300, "misses": 39 },
  "live_reload_clients": 0,
  "proxy": { "errors": 0, "backends": [
    { "route": "/api", "backend": "127.0.0.1:3000", "healthy": true,
      "active": 1, "requests": 812, "failures": 0 } ] }
}
```

**Qui peut le voir ?** Par défaut, uniquement la machine elle-même
(`127.0.0.1`), et jamais une requête relayée par un proxy. Pour y accéder à
distance, définissez `STATUS_AUTH=utilisateur:motdepasse`. Pour le désactiver
complètement : `STATUS=0`.

### 11.4 `/__lith/metrics` : pour Prometheus et Grafana

Les mêmes statistiques au format texte de **Prometheus**, l'outil de
supervision le plus répandu (souvent couplé à **Grafana** pour les graphiques) :

```text
lith_info{version="1.0.0"} 1
lith_uptime_seconds 3600
lith_connections_active 3
lith_requests_total{class="2xx"} 4990
lith_requests_total{class="4xx"} 19
lith_bytes_sent_total 9482635
lith_cache_hits_total 4300
lith_backend_up{route="/api",backend="127.0.0.1:3000"} 1
...
```

Configuration de Prometheus (`prometheus.yml`) :

```yaml
scrape_configs:
  - job_name: lith
    metrics_path: /__lith/metrics
    static_configs:
      - targets: ['127.0.0.1:8090']
    # si STATUS_AUTH est défini :
    # basic_auth: { username: admin, password: motdepasse }
```

---

## Chapitre 12 — Mettre en production

### 12.1 Choisir son installation

| Situation | Solution conseillée |
|-----------|---------------------|
| Un serveur Linux (VPS, Raspberry Pi) | **systemd** (12.3) + HTTPS intégré (`make TLS=1`) ou **Caddy** (12.4) |
| Une plateforme à conteneurs (Docker, Kubernetes, Cloud Run...) | **Docker** (12.5) |
| Test rapide sur un serveur Linux | `lith start -d` / `lith stop` (12.2) |
| Windows | Planificateur de tâches (12.6) |

### 12.2 Le mode démon (Linux/macOS)

```bash
lith start -d
```

LITH se détache du terminal (vous pouvez le fermer) et continue de tourner :

```text
LITH démarre en arrière-plan sur le port 8090.
  Journal : /home/moi/mon-site/lith.log
  PID     : /home/moi/mon-site/lith.pid
  Arrêt   : lith stop
```

`lith stop` (lancé depuis le même dossier, ou avec `-c` vers le même fichier de
configuration) lit le fichier PID, demande l'arrêt propre et attend sa fin.

### 12.3 Avec systemd (recommandé sous Linux)

systemd démarre LITH avec la machine et le relance en cas de problème.
Créez `/etc/systemd/system/lith.service` :

```ini
[Unit]
Description=LITH - serveur web
After=network.target

[Service]
WorkingDirectory=/srv/mon-site
ExecStart=/usr/local/bin/lith start
Restart=on-failure
User=www-data
Group=www-data
# Pour écouter sur le port 80 sans être root :
AmbientCapabilities=CAP_NET_BIND_SERVICE
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
```

> Avec systemd, **n'utilisez pas `-d`** : systemd gère lui-même l'arrière-plan.

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now lith     # démarrer maintenant et à chaque démarrage
systemctl status lith                # état
journalctl -u lith -f                # journal en direct
sudo systemctl restart lith          # après une modification de lith.conf
```

### 12.4 Ajouter HTTPS

Prérequis communs : un nom de domaine qui pointe vers votre serveur, et les
ports 80 et 443 ouverts. Deux méthodes ; la première garde un seul programme,
la seconde automatise complètement les certificats.

#### Méthode A : HTTPS intégré à LITH (`make TLS=1`) + Let's Encrypt

1. Compilez et installez LITH avec HTTPS (chapitre 2.3) :

   ```bash
   make TLS=1 && sudo install -m 755 bin/lith /usr/local/bin/lith
   ```

2. Démarrez d'abord LITH **en HTTP seul** sur le port 80 (service systemd du
   12.3, avec `PORT=80`) : il faut un site joignable pour obtenir le premier
   certificat.

3. Obtenez le certificat avec [certbot](https://certbot.eff.org) en mode
   *webroot* : certbot dépose un fichier dans `public/.well-known/acme-challenge/`,
   que LITH sert (ce dossier n'est jamais caché, ni redirigé vers HTTPS).

   ```bash
   sudo certbot certonly --webroot -w /srv/mon-site/public -d monsite.fr
   ```

4. Les fichiers de `/etc/letsencrypt/live/` ne sont lisibles que par `root`.
   Copiez-les pour l'utilisateur de LITH, à chaque renouvellement, avec un
   petit script `/etc/letsencrypt/renewal-hooks/deploy/lith.sh` (rendez-le
   exécutable avec `chmod +x`) :

   ```bash
   #!/bin/sh
   install -d -o www-data -m 700 /etc/lith
   install -o www-data -m 600 "$RENEWED_LINEAGE/fullchain.pem" "$RENEWED_LINEAGE/privkey.pem" /etc/lith/
   systemctl restart lith      # LITH relit le certificat au démarrage
   ```

   Lancez-le une première fois à la main :
   `sudo RENEWED_LINEAGE=/etc/letsencrypt/live/monsite.fr sh /etc/letsencrypt/renewal-hooks/deploy/lith.sh`.

5. Activez HTTPS dans `lith.conf`, vérifiez, redémarrez :

   ```ini
   PORT=80
   TLS_PORT=443
   TLS_CERT=/etc/lith/fullchain.pem
   TLS_KEY=/etc/lith/privkey.pem
   TLS_REDIRECT=1
   HSTS=300            # passez à 31536000 quand tout fonctionne
   ```

   ```bash
   lith check && sudo systemctl restart lith
   ```

Votre site répond en `https://monsite.fr` ; `http://monsite.fr` y redirige.
certbot renouvelle le certificat automatiquement et le script ci-dessus
redémarre LITH (quelques millisecondes d'interruption). La ligne
`AmbientCapabilities=CAP_NET_BIND_SERVICE` du 12.3 autorise les ports 80 et 443
sans `root`.

#### Méthode B : Caddy devant LITH

[Caddy](https://caddyserver.com) obtient et renouvelle automatiquement des
certificats HTTPS gratuits (Let's Encrypt), sans aucun script. LITH peut alors
rester compilé sans TLS.

1. Configurez LITH pour n'écouter qu'en local :

   ```ini
   HOST=127.0.0.1
   PORT=8090
   ```

2. Installez Caddy, puis écrivez `/etc/caddy/Caddyfile` :

   ```text
   monsite.fr {
       reverse_proxy 127.0.0.1:8090
   }
   ```

3. `sudo systemctl reload caddy`. Votre site est disponible en
   `https://monsite.fr`, avec le cadenas.

Avec **nginx**, l'équivalent est un bloc `location / { proxy_pass http://127.0.0.1:8090; }`
dans un serveur configuré avec un certificat (par exemple via `certbot`).

> 💡 Derrière Caddy ou nginx, toutes les requêtes arrivent à LITH depuis
> `127.0.0.1` : réglez la limitation de débit dans le proxy de devant, et
> utilisez `STATUS_AUTH` si vous voulez consulter `/__lith/status` à distance
> (LITH refuse automatiquement les requêtes relayées sans mot de passe).

### 12.5 Déployer avec Docker

Le `Dockerfile` fourni compile LITH en exécutable statique et produit une image
**`scratch`** (vide) qui ne contient que LITH et le site : environ **330 Ko**,
sans shell ni système d'exploitation à mettre à jour.

```bash
docker build -t lith .

# Servir le site d'exemple
docker run -d -p 8090:8090 --name lith lith

# Servir votre propre site (monté en lecture seule)
docker run -d -p 8090:8090 -v "$PWD/public:/srv/public:ro" lith

# Avec votre propre configuration
docker run -d -p 8090:8090 -v "$PWD/lith.conf:/lith.conf:ro" -v "$PWD/public:/srv/public:ro" lith
```

Dans le conteneur : la configuration est `/lith.conf`, le site `/srv/public`
(voir `docker/lith.conf`). LITH y tourne sans privilèges (utilisateur 65534),
s'arrête proprement avec `docker stop`, et une sonde **HEALTHCHECK** (`lith health`)
indique son état dans `docker ps`.

L'image est compilée **sans HTTPS** pour rester minuscule : sur une plateforme à
conteneurs, le chiffrement est en général assuré par l'*ingress* ou le
répartiteur de charge devant le conteneur.

**docker-compose.yml** :

```yaml
services:
  web:
    build: .
    ports: ["8090:8090"]
    volumes:
      - ./public:/srv/public:ro
    restart: unless-stopped
```

**Kubernetes** : utilisez `/__lith/health` pour les sondes :

```yaml
livenessProbe:
  httpGet: { path: /__lith/health, port: 8090 }
readinessProbe:
  httpGet: { path: /__lith/health, port: 8090 }
```

### 12.6 Sous Windows

Le mode démon n'existe pas sous Windows. Pour que LITH démarre avec la machine :

1. Ouvrez le **Planificateur de tâches** → *Créer une tâche*.
2. *Général* : cochez « Exécuter même si l'utilisateur n'est pas connecté ».
3. *Déclencheurs* : « Au démarrage ».
4. *Actions* : programme `C:\lith\bin\lith.exe`, arguments `start`,
   « Commencer dans » : le dossier de votre site (celui qui contient `lith.conf`).

Pour l'arrêter : `taskkill /IM lith.exe`, ou depuis le Planificateur.

### 12.7 Mettre à jour LITH

1. Téléchargez (ou compilez) la nouvelle version.
2. Lisez le [CHANGELOG](CHANGELOG.md).
3. `lith check` avec votre configuration actuelle.
4. Remplacez l'exécutable et redémarrez (`systemctl restart lith`,
   `lith stop && lith start -d`, ou nouvelle image Docker).

---

## Chapitre 13 — Comment LITH fonctionne à l'intérieur

Ce chapitre s'adresse aux curieux et aux contributeurs. Il n'est pas nécessaire
pour utiliser LITH.

### 13.1 Vue d'ensemble

```text
                    ┌──────────────────────── processus lith ────────────────────────┐
                    │                                                                 │
  connexions TCP ──▶│ thread principal : accept() sur PORT (et TLS_PORT si HTTPS)      │
                    │        │                                                        │
                    │        ▼                                                        │
                    │  file d'attente FIFO (bornée par MAX_PENDING)                    │
                    │        │                                                        │
                    │        ▼                                                        │
                    │  pool de N threads de travail ──▶ lecture requête ──▶ routeur   │
                    │                                                    │            │
                    │        ┌────────────┬──────────────┬───────────────┼─────────┐  │
                    │        ▼            ▼              ▼               ▼         ▼  │
                    │   /__lith/*     AUTH (401)     PROXY          fichiers    erreurs│
                    │   statut,       Basic          backends       statiques          │
                    │   métriques                    round-robin    cache, gzip        │
                    │                                                                 │
                    │ threads annexes : health checks · surveillance live reload ·     │
                    │                   un thread léger par client WebSocket          │
                    └─────────────────────────────────────────────────────────────────┘
```

### 13.2 Le trajet d'une requête

1. **Accept** : le thread principal attend les connexions (`select()` avec un
   délai de 500 ms pour pouvoir s'arrêter proprement), récupère l'adresse IP du
   client et place la connexion dans la file d'attente.
2. **Prise en charge** : un thread de travail libre la retire de la file. Si
   elle arrive sur `TLS_PORT`, une session TLS est créée ; la poignée de main se
   déroule pendant la lecture de la première requête, sous les mêmes délais.
3. **Lecture des en-têtes** : le thread lit jusqu'à trouver la ligne vide
   (`\r\n\r\n`), dans un tampon de 16 Ko maximum, en surveillant les délais.
4. **Analyse** (`http_parser.c`) : ligne de requête, en-têtes, décodage du
   chemin, contrôles de cohérence. Toute anomalie donne une erreur précise (400,
   414, 431, 505...).
5. **Corps** : si `Content-Length` est présent (et ≤ `MAX_BODY_SIZE`), le corps
   est lu entièrement en mémoire.
6. **Routage** (`router.c`), dans cet ordre : limite de débit → méthode connue
   → chemin sûr → routes internes `/__lith/` → `AUTH` → `PROXY` → `OPTIONS` →
   `GET`/`HEAD` sur les fichiers.
7. **Réponse** (`response.c`) : construction des en-têtes communs (date,
   serveur, sécurité, CORS, keep-alive) puis envoi du corps.
8. **Journal** : une ligne d'accès, mise à jour des compteurs.
9. **Suite** : si la connexion reste ouverte (keep-alive), le thread traite la
   requête suivante (éventuellement déjà reçue : *pipelining*), sinon il ferme
   et retourne à la file.

### 13.3 Les modules

| Module | Responsabilité |
|--------|----------------|
| `platform.c` | Seul fichier qui connaît les différences Windows/POSIX : sockets, fichiers en UTF-8, temps, mémoire |
| `server.c` | Écoute (HTTP et HTTPS), pool de threads, boucle de connexion, arrêt propre |
| `tls.c` | E/S d'une connexion (`LithStream`) : socket directe, ou passage par le module TLS si la connexion est chiffrée |
| `tls/` (Rust) | Module HTTPS optionnel (`make TLS=1`) : rustls derrière une petite API C (`tls/lith_tls.h`) |
| `http_parser.c` | Analyse HTTP, types MIME, dates HTTP, plages `Range`, sécurité des chemins |
| `router.c` | Aiguillage et routes internes |
| `static.c` | Fichiers : 304, Range, cache, gzip, envoi par blocs / `sendfile`, liste de dossiers, SPA |
| `response.c` | En-têtes de réponse et pages d'erreur |
| `cache.c` | Table de hachage + LRU + compteur de références |
| `crypto.c` | SHA-1, Base64, CRC-32, compresseur deflate/gzip |
| `security.c` | Authentification Basic, seau à jetons par IP |
| `proxy.c` | Proxy inverse, round-robin, health checks |
| `websocket.c` | Protocole WebSocket et live reload |
| `metrics.c` | Compteurs atomiques et documents JSON / Prometheus |
| `config.c`, `logger.c`, `daemon.c`, `strbuf.c`, `main.c` | Configuration, journaux, démon, chaînes, ligne de commande |

### 13.4 Quelques choix de conception

- **Un pool de threads plutôt qu'une boucle d'événements** (`epoll`/`kqueue`) :
  le code reste simple, lisible et identique sur les trois systèmes. Le nombre
  de threads est élevé par défaut (les threads inactifs ne coûtent presque rien)
  et LITH libère les connexions inactives quand des clients attendent.
- **Un module TLS qui ne touche jamais au réseau** : le code Rust reçoit les
  octets lus par LITH (`rtls_feed`), rend le texte clair (`rtls_read`), chiffre
  les réponses (`rtls_write`) et donne les octets à envoyer
  (`rtls_take_output`). Les sockets, délais, keep-alive et protections
  anti-slowloris restent ceux du code C, identiques en HTTP et en HTTPS. En
  HTTPS, `sendfile` (zéro copie) est remplacé par l'envoi par blocs, puisque
  les données doivent être chiffrées.
- **Un cache à compteur de références** : un thread qui envoie un fichier à un
  client lent ne bloque jamais les autres, et un fichier peut être remplacé ou
  évincé pendant qu'il est envoyé sans risque.
- **Zéro dépendance** : SHA-1, Base64, CRC-32 et la compression gzip sont
  implémentés dans LITH (≈ 400 lignes). Le compresseur utilise LZ77 avec des
  codes de Huffman fixes : un peu moins efficace que zlib au niveau maximum,
  mais très rapide, simple à vérifier, et le résultat est mis en cache.
- **Des chemins UTF-8 partout** : sous Windows, LITH convertit les chemins vers
  les API Unicode (`_wfopen`, `FindFirstFileW`) pour gérer les accents.
- **Arrêt propre** : les signaux (Ctrl+C, `SIGTERM`) ne font que lever un
  drapeau atomique ; les threads le voient et terminent leur travail en cours.
- **Pas d'état partagé modifiable** hors des structures protégées (cache,
  limitation de débit) et des compteurs atomiques : la configuration est en
  lecture seule une fois le serveur démarré.

### 13.5 Limites connues de la 1.0.0

| Limite | Valeur |
|--------|--------|
| Taille des en-têtes d'une requête | 16 Ko, 64 en-têtes |
| Longueur d'une URL | 2048 caractères |
| Clients WebSocket (live reload) | 64 |
| Routes `PROXY` / backends par route | 16 / 8 |
| Règles `AUTH` | 16 |
| Compression à la volée (fichiers hors cache) | fichiers ≤ 8 Mo |
| Liste de dossier (`AUTO_INDEX`) | 10 000 entrées |

---

## Chapitre 14 — Compiler, tester et contribuer

### 14.1 Les commandes `make`

| Commande | Effet |
|----------|-------|
| `make` | Compile `bin/lith` |
| `make test` | Compile et lance les tests unitaires |
| `make integration` | Lance les tests d'intégration (bash + curl) |
| `make DEBUG=1` | Compilation de débogage, avec AddressSanitizer et UBSan (hors Windows) |
| `make STATIC=1` | Exécutable statique (aucune bibliothèque partagée requise) |
| `make TLS=1` | Ajoute HTTPS (module Rust `tls/`, demande `cargo`). À répéter sur chaque commande : `make TLS=1 test`, `make TLS=1 integration` |
| `make CFLAGS="-O3 -march=native"` | Options de compilation personnalisées |
| `make install` / `make uninstall` | Installe / retire `lith` dans `$(PREFIX)/bin` (défaut `/usr/local`) |
| `make clean` | Supprime `build/` et `bin/` |
| `make distclean` | Supprime aussi la compilation Rust (`tls/target/`) |

### 14.2 Les tests

- **Tests unitaires** (`tests/unit_tests.c`) : analyse HTTP (requêtes valides
  et malveillantes), sécurité des chemins, MIME, dates, plages, SHA-1, Base64,
  CRC-32, WebSocket, configuration... Le compresseur gzip est vérifié par un
  mini-décompresseur indépendant écrit dans le test.
- **Tests d'intégration** (`tests/integration.sh`) : de vrais serveurs LITH
  sont lancés, puis interrogés avec `curl` et des connexions TCP brutes :
  fichiers, sécurité, 304, Range, gzip, limites, keep-alive, pipelining,
  slowloris, proxy avec panne de backend, WebSocket, statistiques. Sous
  Linux/macOS, s'y ajoutent l'arrêt par `SIGTERM`, le mode démon, l'absence de
  fuite de descripteurs et un test de charge ApacheBench.
- **Tests HTTPS** (dans `tests/integration.sh`) : avec un exécutable `make TLS=1`,
  un certificat auto-signé jetable est généré avec `openssl` dans un dossier
  temporaire (jamais dans le dépôt), puis sont vérifiés : certificat, ALPN,
  TLS 1.2/1.3, HSTS, redirections `TLS_REDIRECT`, fichiers, Range, gzip, POST,
  keep-alive, proxy, WebSocket `wss://`, refus du HTTP en clair et coupure des
  poignées de main trop lentes. Avec l'exécutable standard, le script vérifie
  que `TLS_PORT` est refusé avec un message clair.

L'intégration continue (GitHub Actions) exécute tout cela sous Linux (dont une
passe sous sanitizers, avec et sans HTTPS), macOS et Windows (avec et sans
HTTPS), et construit l'image Docker, à chaque `push`.

### 14.3 Ajouter une fonctionnalité

1. Lisez [CONTRIBUTING.md](CONTRIBUTING.md) (règles de code et de licence).
2. Créez une branche : `git checkout -b feature/ma-fonction`.
3. Code spécifique à un système → `platform.c`. Nouveau réglage → `config.h`,
   `config_apply()`, `config_print()`, le modèle `LITH_DEFAULT_CONF` et le
   chapitre 5 de ce document.
4. Ajoutez des tests, vérifiez `make test`, `make integration` et
   `make DEBUG=1 && make test DEBUG=1`.
5. Ouvrez une *Pull Request*.

---

## Chapitre 15 — Dépannage et questions fréquentes

**Le port est déjà utilisé.**
Un autre programme écoute déjà sur ce port. Changez de port (`lith start 8091`)
ou trouvez le coupable : `netstat -ano | findstr :8090` (Windows),
`sudo lsof -i :8090` (Linux/macOS).

**« Permission denied » sur le port 80 (Linux).**
Les ports inférieurs à 1024 sont réservés à l'administrateur. Utilisez un port
≥ 1024 derrière Caddy/nginx, ou `AmbientCapabilities=CAP_NET_BIND_SERVICE` avec
systemd (12.3). Ne lancez pas LITH en `root`.

**Ma page s'affiche, mais pas le CSS ou les images.**
Vérifiez les chemins dans votre HTML : `href="/css/style.css"` (depuis la
racine) ou `href="css/style.css"` (relatif à la page). Ouvrez la console du
navigateur (F12 → *Réseau*) pour voir quelles adresses répondent `404`. Attention
aux majuscules : sous Linux, `Logo.png` et `logo.png` sont deux fichiers différents.

**Je modifie un fichier mais le navigateur affiche l'ancienne version.**
Le navigateur utilise son cache. Faites Ctrl+F5, ou utilisez `lith start --dev`.
Vérifiez aussi `CACHE_CONTROL` : avec un `max-age` élevé, le navigateur ne
redemande pas le fichier.

**Les routes de mon application React donnent 404 quand je recharge.**
Activez `SPA_FALLBACK=1` (6.4).

**Mon téléphone n'arrive pas à afficher le site.**
Même Wi-Fi ? `HOST=0.0.0.0` ? Pare-feu autorisé ? Utilisez l'adresse IP de
l'ordinateur, pas `localhost` (6.9).

**`/__lith/status` répond 403.**
C'est voulu : sans `STATUS_AUTH`, il n'est visible que depuis la machine
elle-même, sans proxy devant. Définissez `STATUS_AUTH=utilisateur:motdepasse`.

**Le proxy répond 502.**
L'application derrière ne répond pas : est-elle lancée ? Sur la bonne adresse
et le bon port ? Le journal de LITH indique quel backend est « hors service ».
Testez-la directement : `curl http://127.0.0.1:3000/`.

**`lith stop` ne trouve pas le fichier PID.**
Lancez-le depuis le même dossier que `lith start -d` (ou avec le même `-c`), car
le chemin de `PID_FILE` est relatif à ce dossier.

**Comment voir ce qui se passe en détail ?**
`LOG_LEVEL=debug`, puis observez le journal. `lith check` affiche la
configuration réellement utilisée.

**LITH est-il assez solide pour la production ?**
Oui pour servir des sites et des applications de taille raisonnable : il est
testé automatiquement (dont sous sanitizers mémoire), protège contre les
attaques courantes et s'arrête proprement. Pour un site public, activez HTTPS
(`make TLS=1`, ou un proxy devant : 12.4).

**`lith check` dit « compilé sans HTTPS ».**
Votre exécutable a été compilé sans l'option HTTPS. Recompilez avec
`make TLS=1` (chapitre 2.3), ou retirez `TLS_PORT` de `lith.conf`.

**HTTPS : « certificat illisible » ou « le certificat et la clé ne correspondent pas ».**
`TLS_CERT` doit contenir le certificat au format PEM (texte commençant par
`-----BEGIN CERTIFICATE-----`), de préférence la chaîne complète
(`fullchain.pem`) ; `TLS_KEY` la clé privée qui va avec (`privkey.pem`). Un
fichier `.pfx`/`.p12` (Windows) se convertit avec
`openssl pkcs12 -in site.pfx -nokeys -out cert.pem` et
`openssl pkcs12 -in site.pfx -nocerts -nodes -out key.pem`.

**HTTPS : le navigateur affiche « Votre connexion n'est pas privée ».**
Normal avec un certificat auto-signé (tests en local) : le navigateur ne le
connaît pas. En production, utilisez un certificat d'une autorité reconnue
(Let's Encrypt, gratuit : 12.4). Si l'avertissement parle de nom, le domaine
demandé ne figure pas dans le certificat. Si le site ne se charge pas du tout,
vérifiez que vous utilisez bien `https://` avec `TLS_PORT` (un `http://` sur le
port HTTPS est refusé).

**Puis-je exécuter du PHP ?**
Pas directement. Lancez PHP avec son serveur intégré ou PHP-FPM derrière un
serveur compatible, puis utilisez `PROXY` (chapitre 10).

---

## Chapitre 16 — Feuille de route et choix techniques (dont Rust)

### 16.1 D'où vient la 1.0.0

La 1.0.0 reprend les idées réalistes de la feuille de route d'origine du projet
et les livre dans une première version stable et testée. Les anciens numéros de
version intermédiaires ont été abandonnés : LITH repart proprement de **1.0.0**
et suivra désormais le versionnage sémantique (`MAJEUR.MINEUR.CORRECTIF`).

**Déjà livré dans la 1.0.0** :

| Idée d'origine | État dans la 1.0.0 |
|----------------|--------------------|
| Limitation de débit / anti-DDoS | ✅ `RATE_LIMIT`, seau à jetons par IP, `429` |
| En-têtes de sécurité | ✅ `SECURITY_HEADERS`, `CSP`, `CORS_ORIGIN` |
| Compression gzip | ✅ intégrée, sans zlib, avec cache |
| WebSockets | ✅ RFC 6455, utilisé par le rechargement automatique |
| Proxy inverse | ✅ `PROXY` |
| Répartition de charge | ✅ round-robin + bascule |
| Vérification de l'état des serveurs | ✅ health checks périodiques |
| Authentification Basic | ✅ `AUTH` |
| Tableau de bord de supervision | ✅ `/__lith/status` (JSON), protégé |
| Métriques Prometheus | ✅ `/__lith/metrics` |
| Zero-copy (`sendfile`) | ✅ sous Linux |
| Conteneurisation | ✅ image `scratch` ≈ 330 Ko + health check |

**Livré depuis** : **HTTPS natif** (option `make TLS=1`, module Rust + rustls,
voir 16.4), avec redirection HTTP → HTTPS et HSTS.

### 16.2 Les prochaines étapes recommandées

Par ordre de priorité (valeur pour les utilisateurs / effort) :

1. ~~**HTTPS natif (TLS)**~~ — ✅ livré (`make TLS=1`). Suites possibles :
   obtention automatique des certificats (ACME / Let's Encrypt intégré),
   plusieurs certificats selon le nom de domaine (SNI), backends `https://`
   pour le proxy, HTTPS dans l'image Docker et les exécutables publiés.
2. **Rechargement de la configuration à chaud** (`SIGHUP`) sans couper les
   connexions, y compris le rechargement du certificat après renouvellement.
3. **Redirections et réécritures** configurables (`REDIRECT=/ancien /nouveau 301`),
   puis routage par motifs.
4. **Améliorations du proxy** : réutilisation des connexions vers les backends,
   transmission des WebSockets, corps « chunked ».
5. **CGI / FastCGI** pour exécuter PHP ou Python directement.
6. **Boucle d'événements** (`epoll`, `kqueue`, IOCP) en option, pour les très
   grands nombres de connexions simultanées.
7. **Compression plus forte** (Huffman dynamique) et **Brotli** pré-compressé
   (`fichier.css.br` servi s'il existe).
8. **Jetons JWT** pour protéger des API.
9. **HTTP/2**, puis éventuellement **HTTP/3**, de préférence via une bibliothèque
   éprouvée plutôt qu'une réécriture maison.
10. **Plugins** (bibliothèques chargées dynamiquement) une fois l'API interne stabilisée.

### 16.3 Ce qui a été volontairement écarté

La feuille de route d'origine contenait aussi des idées qui ne relèvent pas
d'un serveur web, ou pas de la technologie actuelle : informatique quantique,
stockage sur ADN, réseau interplanétaire, code qui se réécrit seul par IA,
système d'exploitation dédié, FPGA, etc. Elles ont été retirées : un projet qui
promet tout ne livre rien. La force de LITH est d'être **petit, sûr, portable
et compréhensible** ; chaque nouvelle fonctionnalité doit renforcer ces
qualités. Un « mini-WAF » à base de motifs SQL/XSS a aussi été écarté pour
l'instant : ces filtres bloquent facilement des utilisateurs légitimes et
donnent un faux sentiment de sécurité ; une CSP et un code applicatif correct
protègent mieux.

### 16.4 Faut-il réécrire LITH en Rust ?

**Recommandation : non, pas de réécriture. Gardez le cœur en C11, et faites
intervenir Rust seulement là où il apporte le plus : un module HTTPS optionnel.**

Pourquoi ne **pas** tout réécrire :

- Le code C de la 1.0.0 est désormais modulaire, testé (unitaires, intégration,
  AddressSanitizer, UBSan) et sans dépendance. Une réécriture repartirait de
  zéro et retarderait d'autant les vraies nouveautés.
- L'identité de LITH, c'est la légèreté et la portabilité : un exécutable de
  300 Ko qui se compile avec n'importe quel compilateur C, jusque sur de petits
  appareils embarqués (routeurs, objets connectés). Un équivalent Rust est plus
  gros et demande une chaîne de compilation plus lourde, moins disponible sur
  ces cibles.
- Les risques classiques du C (débordements de tampon, fuites) sont ici
  contenus par une architecture simple, des fonctions bornées, la validation
  stricte des entrées et les tests sous sanitizers.

Où Rust a **vraiment** du sens : **HTTPS**. Le chiffrement et l'analyse des
certificats sont les parties les plus délicates et les plus exposées d'un
serveur ; c'est là qu'une faille mémoire coûte le plus cher. La bibliothèque
Rust **rustls** est moderne, sûre en mémoire et a fait l'objet d'audits.

C'est ce qui a été mis en œuvre :

```text
   LITH (C11)  ──── API C minimale (FFI) ────▶  lith_tls (Rust + rustls)
   - serveur, HTTP, fichiers, proxy            - poignée de main TLS
   - sockets, délais, keep-alive               - chiffrement / déchiffrement
   - tls.c : LithStream (clair ou TLS)         - chargement des certificats
```

- Le dossier `tls/` contient une bibliothèque Rust compilée en bibliothèque
  statique (`crate-type = ["staticlib"]`, moins de 300 lignes), qui expose une
  douzaine de fonctions C déclarées dans `tls/lith_tls.h` : créer un contexte
  avec un certificat et une clé (`rtls_config_new`), accepter une connexion
  (`rtls_accept`), lire (`rtls_feed` + `rtls_read`), écrire (`rtls_write` +
  `rtls_take_output`), fermer (`rtls_close`).
- L'API fonctionne « à tampons » : Rust ne voit jamais la socket. Le C garde la
  main sur le réseau, donc sur les délais et les protections ; aucune
  différence de comportement entre HTTP et HTTPS en dehors du chiffrement.
- Le fournisseur cryptographique est **ring** (pas besoin de cmake ni de nasm),
  la panique Rust est réglée sur `abort` et aucune fonction ne panique au
  travers de la frontière C. Les versions des dépendances sont figées par
  `tls/Cargo.lock` et la compilation utilise `--locked`.
- Activé uniquement avec `make TLS=1` : sans cette option, LITH reste 100 % C,
  sans dépendance, exactement comme avant ; `tls.c` se réduit alors à des
  appels directs à `recv`/`send`.

Alternative écartée : **mbedTLS** (C, léger, conçu pour l'embarqué). Plus simple
à intégrer sans chaîne Rust, mais il aurait ajouté du C à la partie la plus
exposée du serveur ; rustls offre une meilleure garantie de sûreté mémoire. La
séparation par `tls.c` permettrait néanmoins d'ajouter un second module
(mbedTLS) pour des cibles où Rust n'est pas disponible.

En résumé : **C pour le cœur (léger, portable, déjà solide), Rust en module
optionnel pour HTTPS (là où la sûreté mémoire compte le plus)**. Une réécriture
complète ne serait à reconsidérer que si LITH devenait un projet beaucoup plus
vaste (HTTP/2, HTTP/3, plugins...) porté par une équipe à l'aise avec Rust.

---

## Chapitre 17 — Glossaire

| Terme | Définition |
|-------|------------|
| **Backend** | Application placée derrière LITH, à qui il transmet des requêtes (proxy) |
| **Cache** | Copie gardée à portée de main pour aller plus vite (en RAM côté serveur, sur le disque côté navigateur) |
| **Client** | Le programme qui fait la demande : navigateur, `curl`, application mobile... |
| **CORS** | Règles qui autorisent (ou non) un site à lire les réponses d'un autre |
| **CSP** | *Content-Security-Policy* : liste des sources autorisées pour les scripts, styles, images |
| **Démon** | Programme qui tourne en arrière-plan, sans terminal |
| **En-tête** | Ligne d'information `Nom: valeur` d'une requête ou d'une réponse HTTP |
| **ETag** | Étiquette identifiant une version d'un fichier, pour les réponses `304` |
| **gzip** | Format de compression utilisé sur le web |
| **Health check** | Vérification automatique qu'un serveur répond |
| **HTTP / HTTPS** | Le protocole du web / sa version chiffrée |
| **IP (adresse)** | Numéro qui identifie une machine sur un réseau (`192.168.1.20`) |
| **Keep-alive** | Réutiliser une connexion pour plusieurs requêtes |
| **localhost** | Votre propre machine (`127.0.0.1`) |
| **LRU** | *Least Recently Used* : on retire d'abord ce qui a servi le moins récemment |
| **MIME (type)** | Nature d'un contenu (`text/html`, `image/png`...) |
| **PATH** | Liste des dossiers où le système cherche les commandes |
| **PID** | Numéro d'un processus en cours d'exécution |
| **Pipelining** | Envoyer plusieurs requêtes à la suite sans attendre les réponses |
| **Port** | Numéro de « porte » d'un programme réseau sur une machine |
| **Prometheus** | Outil de supervision qui collecte des métriques |
| **Proxy inverse** | Serveur qui reçoit les requêtes et les transmet à un autre serveur |
| **Requête / réponse** | La demande du client / ce que le serveur renvoie |
| **Round-robin** | Répartition à tour de rôle |
| **Slowloris** | Attaque qui occupe un serveur en envoyant des requêtes extrêmement lentement |
| **SPA** | *Single Page Application* : application web monopage (React, Vue...) |
| **Thread** | Fil d'exécution : permet de faire plusieurs choses en même temps dans un programme |
| **TLS** | La technologie de chiffrement derrière HTTPS |
| **URL** | Adresse complète d'une ressource web |
| **WebSocket** | Connexion permanente et bidirectionnelle entre navigateur et serveur |

---

## Annexe A — Aide-mémoire

```bash
lith init mon-site            # créer un projet
lith start                    # démarrer (port de lith.conf, 8090 par défaut)
lith start 8080 --dev         # autre port + rechargement auto
lith start -r dist -p 3000    # autre dossier, autre port
lith start -d && lith stop    # arrière-plan (Linux/macOS)
lith check                    # valider la configuration
lith health                   # le serveur répond-il ?
```

```ini
# lith.conf - les réglages les plus utiles
PORT=8090
PUBLIC_DIR=public
SPA_FALLBACK=1
RATE_LIMIT=50
CSP=default-src 'self'
AUTH=/admin admin:motdepasse
PROXY=/api http://127.0.0.1:3000
STATUS_AUTH=admin:motdepasse
LOG_FILE=lith.log
# HTTPS (exécutable compilé avec : make TLS=1)
TLS_PORT=443
TLS_CERT=fullchain.pem
TLS_KEY=privkey.pem
TLS_REDIRECT=1
HSTS=31536000
```

| Adresse | Rôle |
|---------|------|
| `/__lith/health` | Disponibilité |
| `/__lith/status` | Statistiques JSON |
| `/__lith/metrics` | Prometheus |

---

## Annexe B — Codes de réponse HTTP utilisés par LITH

| Code | Nom | Quand LITH l'envoie |
|------|-----|---------------------|
| 101 | Switching Protocols | Ouverture d'un WebSocket (live reload) |
| 200 | OK | Fichier envoyé |
| 204 | No Content | Réponse à `OPTIONS` |
| 206 | Partial Content | Morceau de fichier (`Range`) |
| 301 | Moved Permanently | `/dossier` → `/dossier/` ; `http://` → `https://` (`TLS_REDIRECT`, `GET`/`HEAD`) |
| 304 | Not Modified | Le navigateur a déjà la bonne version |
| 308 | Permanent Redirect | `http://` → `https://` pour `POST`, `PUT`... (`TLS_REDIRECT`) |
| 400 | Bad Request | Requête mal formée, `Host` manquant, chemin invalide |
| 401 | Unauthorized | Mot de passe requis (`AUTH`, `STATUS_AUTH`) |
| 403 | Forbidden | Tentative de sortir du dossier public ; statut demandé à distance |
| 404 | Not Found | Fichier absent ou caché |
| 405 | Method Not Allowed | `POST`/`PUT`/`DELETE` sur un fichier statique |
| 408 | Request Timeout | Requête trop lente (`REQUEST_TIMEOUT`) |
| 411 | Length Required | Corps « chunked » sans `Content-Length` |
| 413 | Content Too Large | Corps supérieur à `MAX_BODY_SIZE` |
| 414 | URI Too Long | URL de plus de 2048 caractères |
| 416 | Range Not Satisfiable | Morceau demandé hors du fichier |
| 429 | Too Many Requests | `RATE_LIMIT` dépassé |
| 431 | Request Header Fields Too Large | En-têtes de plus de 16 Ko |
| 500 | Internal Server Error | Problème inattendu (rare) |
| 501 | Not Implemented | Méthode inconnue, encodage non géré, WebSocket vers un backend |
| 502 | Bad Gateway | Aucun backend du proxy ne répond correctement |
| 503 | Service Unavailable | File d'attente pleine (`MAX_PENDING`) |
| 504 | Gateway Timeout | Backend trop lent (`PROXY_TIMEOUT`) |
| 505 | HTTP Version Not Supported | Version autre que HTTP/1.x |

---

*LITH 1.0.0 — FomaDev. Distribué sous la [FomaDev Public License](LICENSE).*
