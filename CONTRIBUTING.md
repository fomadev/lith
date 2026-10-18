# Contribuer à LITH

Merci de votre intérêt pour **LITH** ! Pour garder un code portable, sûr et
cohérent, chaque contribution suit les règles ci-dessous.

En contribuant à ce dépôt, vous acceptez que votre code soit régi par la
**Fordima Public License (FPL)** et intégré à l'écosystème officiel FomaDev.

---

## Licence et conformité

1. **Forks temporaires uniquement** : un fork sert à proposer des modifications
   au dépôt officiel. Les forks permanents et indépendants destinés à contourner
   l'architecture ou la licence FomaDev sont interdits.
2. **Restrictions commerciales** : les versions modifiées de LITH, son code ou
   ses dérivés ne peuvent être distribués, revendus ou intégrés à un logiciel
   commercial concurrent sans autorisation commerciale préalable.

---

## Préparer son environnement

- Un compilateur **C11** : `gcc` ou `clang` (Linux/macOS), **MinGW-w64** sous
  Windows (MSYS2 ou w64devkit).
- **GNU Make**.
- Pour les tests d'intégration : `bash` et `curl` (Git Bash convient sous Windows).

```bash
git clone https://github.com/fomadev/lith.git
cd lith
git checkout -b feature/ma-fonctionnalite   # ou fix/mon-correctif
make
make test
make integration
```

---

## Organisation du code

| Fichier | Rôle |
|---------|------|
| `src/main.c` | Ligne de commande (`start`, `stop`, `init`, `check`, `health`) |
| `src/server.c` | Écoute réseau, pool de threads, cycle de vie d'une connexion |
| `src/http_parser.c` | Analyse HTTP, MIME, dates, chemins, Range |
| `src/router.c` | Aiguillage des requêtes |
| `src/static.c` | Fichiers statiques (304, Range, gzip, cache, listes de dossiers) |
| `src/response.c` | Construction des réponses et pages d'erreur |
| `src/cache.c` | Cache mémoire LRU |
| `src/crypto.c` | SHA-1, Base64, CRC-32, compression gzip |
| `src/security.c` | Authentification Basic, limitation de débit |
| `src/proxy.c` | Proxy inverse, répartition de charge, health checks |
| `src/websocket.c` | WebSocket et rechargement automatique |
| `src/metrics.c` | Statistiques `/__lith/status` et `/__lith/metrics` |
| `src/config.c` | Lecture et validation de `lith.conf` |
| `src/platform.c` | Tout ce qui dépend du système d'exploitation |
| `src/logger.c`, `src/strbuf.c`, `src/daemon.c` | Journaux, chaînes extensibles, mode démon |

Chaque fichier `src/x.c` a son en-tête dans `include/`. Un nouveau fichier
placé dans `src/` est compilé automatiquement par le `Makefile`.

---

## Règles techniques

### Portabilité
LITH doit compiler **sans avertissement** (`-Wall -Wextra`) et fonctionner à
l'identique sous Linux, macOS et Windows. Le code spécifique à un système va
dans `src/platform.c`, protégé par `#ifdef _WIN32`. Ailleurs, utilisez les
abstractions (`socket_t`, `lith_stat`, `lith_fopen`, `lith_send_all`...).

### Zéro dépendance
Aucune bibliothèque externe n'est ajoutée au cœur de LITH. Une fonctionnalité
qui en exigerait une doit être **optionnelle** et activée à la compilation.

### Sûreté mémoire
- Fonctions bornées (`snprintf`, `memcpy` avec tailles vérifiées), jamais `strcpy`/`sprintf`.
- Toute donnée venant du réseau est considérée comme hostile : validez les longueurs.
- Chaque `malloc` a son `free`, y compris sur les chemins d'erreur.
- Vérifiez avec `make DEBUG=1 && make test DEBUG=1` (AddressSanitizer/UBSan sous Linux/macOS).

### Tests
Toute nouvelle fonctionnalité est accompagnée de tests :
- fonction pure (analyse, calcul) → `tests/unit_tests.c` ;
- comportement réseau → `tests/integration.sh`.

### Messages de commit
Phrase courte à l'impératif décrivant le changement :

```text
Ajoute la prise en charge de If-Range pour les requêtes partielles
```

et non : `correction`.

---

## Signaler un problème

Ouvrez une *Issue* GitHub en indiquant :
- système d'exploitation et compilateur ;
- étapes pour reproduire ;
- comportement attendu et comportement observé ;
- journaux de LITH (lancez avec `LOG_LEVEL=debug` si possible) et sortie de `lith check`.

Pour une faille de sécurité, contactez FomaDev en privé avant toute publication.

---

## Questions

Pour toute question sur l'architecture, la licence ou une collaboration
commerciale, contactez directement Fordi (FomaDev).
