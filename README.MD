# LITH - Serveur HTTP Léger 🚀

LITH est un serveur HTTP minimaliste, performant et multi-threadé écrit en **C11**. Il est conçu pour être extrêmement léger et portable, fonctionnant parfaitement sur **Windows** (via MinGW/Winsock2) et **Linux**.

## ✨ Caractéristiques

- **Multi-threadé** : Utilise pthreads pour gérer plusieurs connexions simultanées.
- **Service de fichiers statiques** : Sert automatiquement les fichiers depuis le dossier `public/`.
- **Cross-Platform** : Support natif pour Windows et Linux.
- **Léger** : Zéro dépendance externe (uniquement les bibliothèques C standards).
- **Logs intégrés** : Système de journalisation en temps réel avec codes couleurs dans le terminal.

## 🛠️ Installation & Compilation

### Prérequis
- **GCC** (MinGW-w64 pour Windows ou GCC pour Linux)
- **Make**

### Compilation
Ouvrez votre terminal à la racine du projet et lancez :
```bash
make
```
Cela créera le dossier `bin/` et l'exécutable `lith.exe` (ou `lith`).

## 🚀 Utilisation
1. Préparez vos fichiers : Placez vos fichiers HTML, CSS ou JS dans le dossier public/.

2. Lancez le serveur :
```bash
bin/lith.exe
```
ou
```bash
bin/lith
```
3. Accédez au serveur : Ouvrez votre navigateur et allez sur http://localhost:8080.
**Note** : Vous pouvez spécifier un port personnalisé en argument : `bin/lith.exe 9090` ou `bin/lith 9090`

## 📂 Structure du Projet
* `src/` : Logique d'implémentation (.c).

* `include/` : Définitions des en-têtes (.h).

* `public/` : Racine du serveur web (placez votre index.html ici).

* `bin/` : Exécutables compilés.

## 📝 Configuration
La configuration du serveur se trouve dans `include/server.h` :

`DEFAULT_PORT` : 8080

`BUFFER_SIZE` : 4096 octets

`BACKLOG` : 10 connexions en attente

## ⚖️ Licence - MIT
Ce projet est open-source et libre d'utilisation.