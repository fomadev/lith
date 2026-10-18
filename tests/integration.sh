#!/usr/bin/env bash
# ==============================================================================
#  LITH - Tests d'intégration (bash + curl)
#
#  Lance de vrais serveurs LITH sur des ports libres et vérifie leur
#  comportement de bout en bout. Fonctionne sous Linux, macOS et Windows
#  (Git Bash / MSYS2). Les tests propres à UNIX (signaux, démon, fuites de
#  descripteurs) sont ignorés ailleurs.
#
#  Utilisation : make integration   (ou : bash tests/integration.sh [chemin/vers/lith])
# ==============================================================================

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LITH="${1:-}"
if [ -z "$LITH" ]; then
    if [ -x "$ROOT/bin/lith" ]; then LITH="$ROOT/bin/lith"; else LITH="$ROOT/bin/lith.exe"; fi
fi
[ -x "$LITH" ] || { echo "Exécutable introuvable : $LITH (lancez make)"; exit 1; }

PORT=${LITH_TEST_PORT:-18090}
BACKEND_PORT=$((PORT + 1))
DEAD_PORT=$((PORT + 2))
BASE="http://127.0.0.1:$PORT"
WORK="$(mktemp -d 2>/dev/null || mktemp -d -t lith)"
PASS=0
FAIL=0
UNIX=1
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) UNIX=0 ;; esac

cleanup() {
    [ -n "${SERVER_PID:-}" ] && kill "$SERVER_PID" 2>/dev/null
    [ -n "${BACKEND_PID:-}" ] && kill "$BACKEND_PID" 2>/dev/null
    [ -n "${TLS_PID:-}" ] && kill "$TLS_PID" 2>/dev/null
    [ -n "${LIM_PID:-}" ] && kill "$LIM_PID" 2>/dev/null
    if [ $UNIX -eq 0 ]; then taskkill //F //IM "$(basename "$LITH")" >/dev/null 2>&1 || true; fi
    rm -rf "$WORK"
}
trap cleanup EXIT

ok()   { PASS=$((PASS + 1)); printf "  \033[32mOK\033[0m   %s\n" "$1"; }
ko()   { FAIL=$((FAIL + 1)); printf "  \033[31mÉCHEC\033[0m %s (%s)\n" "$1" "$2"; }
expect() { # expect "description" "attendu" "obtenu"
    if [ "$2" = "$3" ]; then ok "$1"; else ko "$1" "attendu '$2', obtenu '$3'"; fi
}
code() { curl -s -o /dev/null -w "%{http_code}" "$@"; }

wait_up() {
    for _ in $(seq 1 50); do
        [ "$(code "http://127.0.0.1:$1/__lith/health")" = "200" ] && return 0
        sleep 0.1
    done
    return 1
}

# Envoie des octets bruts sur une connexion TCP et affiche la réponse.
raw() {
    exec 3<>"/dev/tcp/127.0.0.1/$PORT" || return 1
    printf "$1" >&3
    timeout "${2:-3}" cat <&3
    exec 3<&-
}

# ------------------------------------------------------------------------------
echo "Préparation dans $WORK"
mkdir -p "$WORK/site/public/docs" "$WORK/site/public/admin" "$WORK/site/public/.git" "$WORK/backend/public/api"
cd "$WORK/site" || exit 1
"$LITH" init . >/dev/null
echo "secret" > public/.env
echo "[core]" > public/.git/config
echo "documentation" > public/docs/notes.txt
echo "<h1>Zone admin</h1>" > public/admin/index.html
echo "<h1>Page perso 404</h1>" > public/404.html
for i in $(seq 1 300); do echo "<p>Ligne $i : LITH sert des fichiers rapidement et simplement.</p>"; done > public/long.txt
head -c 200000 /dev/urandom > public/data.bin
echo '{"backend":"ok"}' > "$WORK/backend/public/api/hello.json"
printf 'PORT=%s\nACCESS_LOG=0\n' "$BACKEND_PORT" > "$WORK/backend/lith.conf"

cat > lith.conf <<EOF
PORT=$PORT
REQUEST_TIMEOUT=2
KEEPALIVE_TIMEOUT=2
MAX_BODY_SIZE=64K
AUTO_INDEX=1
SPA_FALLBACK=1
CORS_ORIGIN=*
AUTH=/admin admin:s3cret
PROXY=/api http://127.0.0.1:$DEAD_PORT, http://127.0.0.1:$BACKEND_PORT
HEALTH_CHECK_INTERVAL=1
LIVE_RELOAD=1
ACCESS_LOG=1
EOF

echo "== Commandes"
expect "lith version" "LITH 1.0.0" "$("$LITH" version)"
"$LITH" check >/dev/null 2>&1; expect "lith check (config valide)" "0" "$?"
echo "PORT=abc" > bad.conf
"$LITH" check -c bad.conf >/dev/null 2>&1; expect "lith check (config invalide)" "1" "$?"
"$LITH" health "$PORT" >/dev/null 2>&1; expect "lith health (serveur arrêté)" "1" "$?"

(cd "$WORK/backend" && exec "$LITH" start > backend.log 2>&1) & BACKEND_PID=$!
"$LITH" start > server.log 2>&1 & SERVER_PID=$!
if ! wait_up "$PORT" || ! wait_up "$BACKEND_PORT"; then
    echo "Le serveur n'a pas démarré :"; cat server.log; exit 1
fi
"$LITH" health "$PORT" >/dev/null 2>&1; expect "lith health (serveur en marche)" "0" "$?"

echo "== Fichiers statiques"
expect "GET /" "200" "$(code "$BASE/")"
expect "HEAD /" "200" "$(code -I "$BASE/")"
expect "type MIME HTML" "text/html; charset=utf-8" "$(curl -s -o /dev/null -w '%{content_type}' "$BASE/")"
expect "404 avec page personnalisée" "<h1>Page perso 404</h1>" "$(curl -s "$BASE/absent.png")"
expect "SPA_FALLBACK sur /profil/42" "200" "$(code "$BASE/profil/42")"
expect "redirection /docs -> /docs/" "301" "$(code "$BASE/docs")"
expect "AUTO_INDEX liste notes.txt" "1" "$(curl -s "$BASE/docs/" | grep -c 'notes.txt')"
curl -s "$BASE/data.bin" -o "$WORK/dl.bin"
cmp -s "$WORK/dl.bin" public/data.bin && ok "fichier binaire identique" || ko "fichier binaire identique" "contenu différent"

echo "== Sécurité"
expect "fichier caché .env" "404" "$(code "$BASE/.env")"
expect "dossier caché .git" "404" "$(code "$BASE/.git/config")"
expect "remontée ../" "403" "$(code --path-as-is "$BASE/../../etc/passwd")"
expect "remontée encodée %2e%2e" "403" "$(code --path-as-is "$BASE/%2e%2e/%2e%2e/etc/passwd")"
expect "octet nul %00" "400" "$(code "$BASE/index.html%00.txt")"
expect "en-tête nosniff" "1" "$(curl -s -D - -o /dev/null "$BASE/" | grep -ci 'x-content-type-options: nosniff')"
expect "AUTH sans identifiants" "401" "$(code "$BASE/admin/")"
expect "AUTH mauvais mot de passe" "401" "$(code -u admin:faux "$BASE/admin/")"
expect "AUTH correct" "200" "$(code -u admin:s3cret "$BASE/admin/")"
expect "/administrateur non protégé" "404" "$(code "$BASE/administrateur.png")"
expect "méthode POST sur fichier" "405" "$(code -X POST -d x=1 "$BASE/index.html")"
expect "méthode inconnue" "501" "$(code -X BREW "$BASE/")"

echo "== Protocole HTTP"
ETAG=$(curl -s -D - -o /dev/null "$BASE/long.txt" | grep -i '^etag:' | cut -d' ' -f2 | tr -d '\r')
expect "If-None-Match -> 304" "304" "$(code -H "If-None-Match: $ETAG" "$BASE/long.txt")"
expect "Range 0-99 -> 206" "206 100" "$(curl -s -o /dev/null -w '%{http_code} %{size_download}' -H 'Range: bytes=0-99' "$BASE/data.bin")"
expect "Range hors fichier -> 416" "416" "$(code -H 'Range: bytes=999999-' "$BASE/data.bin")"
expect "Range suffixe (-50) -> 206" "206 50" "$(curl -s -o /dev/null -w '%{http_code} %{size_download}' -H 'Range: bytes=-50' "$BASE/data.bin")"
LASTMOD=$(curl -s -D - -o /dev/null "$BASE/long.txt" | grep -i '^last-modified:' | cut -d' ' -f2- | tr -d '\r')
expect "If-Modified-Since -> 304" "304" "$(code -H "If-Modified-Since: $LASTMOD" "$BASE/long.txt")"
expect "OPTIONS (CORS préliminaire) -> 204" "204" "$(code -X OPTIONS -H 'Origin: https://a.fr' -H 'Access-Control-Request-Method: GET' "$BASE/")"
expect "en-tête CORS" "1" "$(curl -s -D - -o /dev/null "$BASE/" | grep -ci 'access-control-allow-origin: \*')"
GZ=$(curl -s -o /dev/null -w '%{size_download}' -H 'Accept-Encoding: gzip' "$BASE/long.txt")
RAWSZ=$(curl -s -o /dev/null -w '%{size_download}' "$BASE/long.txt")
[ "$GZ" -lt $((RAWSZ / 3)) ] && ok "gzip réduit la taille ($RAWSZ -> $GZ octets)" || ko "gzip" "$RAWSZ -> $GZ"
curl -s --compressed "$BASE/long.txt" -o "$WORK/long.txt"
cmp -s "$WORK/long.txt" public/long.txt && ok "gzip décompressé identique" || ko "gzip décompressé identique" "contenu différent"
expect "POST /__lith/echo" "21" "$(curl -s -X POST -H 'Content-Type: application/json' -d '{"test":"ci_payload"}' "$BASE/__lith/echo" | sed 's/.*"bytes_received":\([0-9]*\).*/\1/')"
head -c 70000 /dev/zero > "$WORK/big.bin"
expect "corps trop gros -> 413" "413" "$(code --data-binary @"$WORK/big.bin" "$BASE/__lith/echo")"
BIGH=$(head -c 20000 /dev/zero | tr '\0' 'a')
expect "en-têtes trop gros -> 431" "431" "$(code -H "X-Big: $BIGH" "$BASE/")"
expect "HTTP/1.1 sans Host -> 400" "HTTP/1.1 400 Bad Request" "$(raw 'GET / HTTP/1.1\r\n\r\n' | head -1 | tr -d '\r')"
expect "HTTP/2.0 -> 505" "HTTP/1.1 505 HTTP Version Not Supported" "$(raw 'GET / HTTP/2.0\r\nHost: x\r\n\r\n' | head -1 | tr -d '\r')"
expect "pipelining (2 requêtes)" "2" "$(raw 'GET /__lith/health HTTP/1.1\r\nHost: x\r\n\r\nGET /__lith/health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n' | grep -c 'HTTP/1.1 200')"
REUSE=$(curl -sv "$BASE/" "$BASE/" -o /dev/null -o /dev/null 2>&1 | grep -ci 're-using\|reusing')
[ "$REUSE" -ge 1 ] && ok "keep-alive : connexion réutilisée" || ko "keep-alive" "pas de réutilisation"
START=$(date +%s)
LINE=$(raw 'GET / HTTP/1.1\r\nHost: x\r\n' 8 | head -1 | tr -d '\r')
DUR=$(( $(date +%s) - START ))
expect "requête incomplète -> 408" "HTTP/1.1 408 Request Timeout" "$LINE"
[ "$DUR" -le 5 ] && ok "anti-slowloris : coupé en ${DUR}s" || ko "anti-slowloris" "${DUR}s"

echo "== Proxy inverse"
expect "proxy vers backend" '{"backend":"ok"}' "$(curl -s "$BASE/api/hello.json")"
ALL=$(for _ in 1 2 3 4; do printf '%s ' "$(code "$BASE/api/hello.json")"; done)
expect "bascule si un backend est mort" "200 200 200 200 " "$ALL"
expect "404 transmis par le backend" "404" "$(code "$BASE/api/absent")"

echo "== Live reload & WebSocket"
expect "script injecté dans le HTML" "1" "$(curl -s "$BASE/" | grep -c '__lith/livereload')"
WS=$(curl -s -i -N --max-time 1 -H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' \
     -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' "$BASE/__lith/livereload" | tr -d '\r')
expect "poignée de main 101" "1" "$(echo "$WS" | grep -c '101 Switching Protocols')"
expect "Sec-WebSocket-Accept (RFC 6455)" "1" "$(echo "$WS" | grep -c 's3pPLMBiTxaQ9kYGzzhZRbK+xOo=')"
# Un fichier modifié doit envoyer la trame texte "reload" (0x81, longueur 6) aux navigateurs connectés.
curl -s -N --max-time 5 -H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' \
     -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' "$BASE/__lith/livereload" > "$WORK/ws.bin" & WS_PID=$!
sleep 1.5
echo "modifié" > public/touch.txt
wait "$WS_PID" 2>/dev/null
expect "live reload : trame 'reload' après modification" "1" "$(grep -c 'reload' "$WORK/ws.bin")"

echo "== Supervision"
expect "/__lith/status (JSON)" "1" "$(curl -s "$BASE/__lith/status" | grep -c '"version": "1.0.0"')"
expect "/__lith/metrics (Prometheus)" "1" "$(curl -s "$BASE/__lith/metrics" | grep -c '^lith_info{version="1.0.0"} 1')"
expect "statut refusé si relayé par un proxy" "403" "$(code -H "X-Forwarded-For: 203.0.113.9" "$BASE/__lith/status")"
expect "journal d'accès écrit" "1" "$([ "$(grep -c '"GET / HTTP/1.1" 200' server.log)" -ge 1 ] && echo 1 || echo 0)"

echo "== Limitation de débit, statut protégé, en-têtes configurables"
LIM_PORT=$((PORT + 5))
LIM_BASE="http://127.0.0.1:$LIM_PORT"
mkdir -p "$WORK/lim/public"
echo "<h1>lim</h1>" > "$WORK/lim/public/index.html"
cat > "$WORK/lim/lith.conf" <<EOF
PORT=$LIM_PORT
RATE_LIMIT=3
RATE_BURST=3
STATUS_AUTH=adm:pw
CSP=default-src 'self'
CACHE_CONTROL=max-age=60
LOG_FILE=lim.log
EOF
(cd "$WORK/lim" && exec "$LITH" start > console.log 2>&1) & LIM_PID=$!
if wait_up "$LIM_PORT"; then
    sleep 1.1   # seau à jetons plein après la sonde de démarrage
    expect "en-tête CSP" "1" "$(curl -s -D - -o /dev/null "$LIM_BASE/" | grep -ci "content-security-policy: default-src 'self'")"
    expect "en-tête Cache-Control" "1" "$(curl -s -D - -o /dev/null "$LIM_BASE/" | grep -ci 'cache-control: max-age=60')"
    # Rafale : on garde les en-têtes de la première réponse 429 (le seau se remplit à 3 jetons/s).
    LIMITED=""
    for _ in $(seq 1 10); do
        H=$(curl -s -D - -o /dev/null "$LIM_BASE/")
        case "$H" in "HTTP/1.1 429"*) LIMITED="$H"; break ;; esac
    done
    expect "RATE_LIMIT : 429 après la rafale" "1" "$([ -n "$LIMITED" ] && echo 1 || echo 0)"
    expect "RATE_LIMIT : Retry-After" "1" "$(echo "$LIMITED" | grep -ci '^retry-after: 1')"
    sleep 1.5
    expect "STATUS_AUTH sans mot de passe -> 401" "401" "$(code "$LIM_BASE/__lith/status")"
    expect "STATUS_AUTH avec mot de passe -> 200" "200" "$(code -u adm:pw "$LIM_BASE/__lith/status")"
    expect "LOG_FILE : journal écrit dans le fichier" "1" "$([ "$(grep -c 'GET / HTTP/1.1' "$WORK/lim/lim.log")" -ge 1 ] && echo 1 || echo 0)"
else
    ko "serveur avec RATE_LIMIT" "n'a pas démarré"
fi
kill "$LIM_PID" 2>/dev/null; wait "$LIM_PID" 2>/dev/null; LIM_PID=""

echo "== HTTPS"
TLS_HTTP_PORT=$((PORT + 3))
TLS_PORT=$((PORT + 4))
TLS_BASE="https://localhost:$TLS_PORT"
mkdir -p "$WORK/tls/public/.well-known/acme-challenge"
cp public/data.bin public/long.txt public/index.html "$WORK/tls/public/"
if "$LITH" check 2>/dev/null | grep -q "compilé sans HTTPS"; then
    # Exécutable sans TLS : HTTPS doit être refusé avec un message clair, jamais ignoré.
    printf 'TLS_PORT=%s\nTLS_CERT=c.pem\nTLS_KEY=k.pem\n' "$TLS_PORT" > "$WORK/tls/lith.conf"
    OUT=$(cd "$WORK/tls" && "$LITH" check 2>&1); RC=$?
    expect "TLS_PORT refusé sans make TLS=1" "1" "$RC"
    expect "message : recompiler avec make TLS=1" "1" "$(echo "$OUT" | grep -c 'make TLS=1`')"
    echo "  (exécutable compilé sans HTTPS : compilez avec make TLS=1 pour les tests HTTPS complets)"
elif ! command -v openssl >/dev/null 2>&1; then
    ko "HTTPS" "openssl est nécessaire pour générer le certificat de test"
else
    # Certificat auto-signé jetable : généré ici, supprimé avec $WORK, jamais commité.
    (cd "$WORK/tls" && MSYS_NO_PATHCONV=1 openssl req -x509 -newkey rsa:2048 -nodes -days 2 \
        -keyout key.pem -out cert.pem -subj "/CN=localhost" \
        -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" >/dev/null 2>&1)
    [ -s "$WORK/tls/cert.pem" ] && ok "certificat de test généré" || ko "certificat de test" "openssl a échoué"
    echo "pas un certificat" > "$WORK/tls/bad.pem"
    cat > "$WORK/tls/lith.conf" <<EOF
PORT=$TLS_HTTP_PORT
TLS_PORT=$TLS_PORT
TLS_CERT=cert.pem
TLS_KEY=key.pem
TLS_REDIRECT=1
HSTS=600
REQUEST_TIMEOUT=2
LIVE_RELOAD=1
PROXY=/api http://127.0.0.1:$BACKEND_PORT
EOF
    sed 's/^TLS_KEY=.*/TLS_KEY=bad.pem/' "$WORK/tls/lith.conf" > "$WORK/tls/bad.conf"
    (cd "$WORK/tls" && "$LITH" check >/dev/null 2>&1); expect "lith check (certificat valide)" "0" "$?"
    (cd "$WORK/tls" && "$LITH" check -c bad.conf >/dev/null 2>&1); expect "lith check (clé invalide)" "1" "$?"

    (cd "$WORK/tls" && exec "$LITH" start > server.log 2>&1) & TLS_PID=$!
    if ! wait_up "$TLS_HTTP_PORT"; then
        ko "serveur HTTPS" "n'a pas démarré"; cat "$WORK/tls/server.log"
    else
        CA="--cacert $WORK/tls/cert.pem"
        expect "GET https:// (certificat vérifié)" "200 0" "$(curl -s $CA -o /dev/null -w '%{http_code} %{ssl_verify_result}' "$TLS_BASE/")"
        curl -s -o /dev/null "$TLS_BASE/"; expect "certificat inconnu refusé par le client" "60" "$?"
        expect "ALPN http/1.1" "1" "$(curl -sv $CA -o /dev/null "$TLS_BASE/" 2>&1 | grep -ci 'ALPN.*accepted.*http/1.1')"
        expect "TLS 1.2 accepté" "200" "$(code $CA --tlsv1.2 --tls-max 1.2 "$TLS_BASE/")"
        expect "en-tête HSTS en HTTPS" "1" "$(curl -s $CA -D - -o /dev/null "$TLS_BASE/" | grep -ci 'strict-transport-security: max-age=600')"
        expect "pas de HSTS en HTTP" "0" "$(curl -s -D - -o /dev/null "http://127.0.0.1:$TLS_HTTP_PORT/__lith/health" | grep -ci 'strict-transport-security')"
        expect "TLS_REDIRECT : GET -> 301" "301 https://localhost:$TLS_PORT/docs?x=1" \
            "$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' "http://localhost:$TLS_HTTP_PORT/docs?x=1")"
        expect "TLS_REDIRECT : POST -> 308" "308" "$(code -X POST -d a=1 "http://localhost:$TLS_HTTP_PORT/__lith/echo")"
        expect "TLS_REDIRECT : santé reste en HTTP" "200" "$(code "http://127.0.0.1:$TLS_HTTP_PORT/__lith/health")"
        expect "TLS_REDIRECT : ACME reste en HTTP" "404" "$(code "http://127.0.0.1:$TLS_HTTP_PORT/.well-known/acme-challenge/jeton")"
        expect "TLS_REDIRECT : Host invalide -> 400" "400" "$(code -H 'Host: a"b' "http://127.0.0.1:$TLS_HTTP_PORT/")"
        expect "redirection suivie jusqu'en HTTPS" "200 $TLS_BASE/" "$(curl -sL $CA -o /dev/null -w '%{http_code} %{url_effective}' "http://localhost:$TLS_HTTP_PORT/")"
        curl -s $CA "$TLS_BASE/data.bin" -o "$WORK/tls-dl.bin"
        cmp -s "$WORK/tls-dl.bin" public/data.bin && ok "HTTPS : fichier binaire identique" || ko "HTTPS : fichier binaire" "contenu différent"
        expect "HTTPS : Range 0-99 -> 206" "206 100" "$(curl -s $CA -o /dev/null -w '%{http_code} %{size_download}' -H 'Range: bytes=0-99' "$TLS_BASE/data.bin")"
        curl -s $CA --compressed "$TLS_BASE/long.txt" -o "$WORK/tls-long.txt"
        cmp -s "$WORK/tls-long.txt" public/long.txt && ok "HTTPS : gzip décompressé identique" || ko "HTTPS : gzip" "contenu différent"
        head -c 50000 /dev/zero > "$WORK/tls-body.bin"
        expect "HTTPS : POST 50 Ko" "50000" "$(curl -s $CA --data-binary @"$WORK/tls-body.bin" "$TLS_BASE/__lith/echo" | sed 's/.*"bytes_received":\([0-9]*\).*/\1/')"
        REUSE=$(curl -sv $CA "$TLS_BASE/" "$TLS_BASE/" -o /dev/null -o /dev/null 2>&1 | grep -ci 're-using\|reusing')
        [ "$REUSE" -ge 1 ] && ok "HTTPS : keep-alive" || ko "HTTPS : keep-alive" "pas de réutilisation"
        expect "HTTPS : proxy vers backend" '{"backend":"ok"}' "$(curl -s $CA "$TLS_BASE/api/hello.json")"
        WSS=$(curl -s $CA -i -N --max-time 1 -H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' \
              -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' "$TLS_BASE/__lith/livereload" | tr -d '\r')
        expect "HTTPS : WebSocket (wss://) 101" "1" "$(echo "$WSS" | grep -c 's3pPLMBiTxaQ9kYGzzhZRbK+xOo=')"
        expect "HTTP en clair sur le port HTTPS : rejeté" "000" "$(curl -s -m 3 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$TLS_PORT/")"
        START=$(date +%s)
        (exec 3<>"/dev/tcp/127.0.0.1/$TLS_PORT" && printf '\026\003\001' >&3 && timeout 8 cat <&3 >/dev/null; exec 3<&-) 2>/dev/null
        DUR=$(( $(date +%s) - START ))
        [ "$DUR" -le 5 ] && ok "HTTPS anti-slowloris : poignée de main coupée en ${DUR}s" || ko "HTTPS anti-slowloris" "${DUR}s"
        expect "serveur HTTPS toujours en vie" "200" "$(code $CA "$TLS_BASE/__lith/health")"
    fi
    kill "$TLS_PID" 2>/dev/null; wait "$TLS_PID" 2>/dev/null; TLS_PID=""
fi

if [ $UNIX -eq 1 ]; then
    echo "== Spécifique UNIX"
    FD1=$(ls /proc/$SERVER_PID/fd 2>/dev/null | wc -l)
    for _ in $(seq 1 30); do code "$BASE/" >/dev/null; code "$BASE/api/hello.json" >/dev/null; done
    sleep 1
    FD2=$(ls /proc/$SERVER_PID/fd 2>/dev/null | wc -l)
    if [ -d /proc/$SERVER_PID ]; then
        [ "$FD2" -le $((FD1 + 3)) ] && ok "aucune fuite de descripteurs ($FD1 -> $FD2)" || ko "fuite de descripteurs" "$FD1 -> $FD2"
    fi
    if command -v ab >/dev/null 2>&1; then
        ab -q -k -n 2000 -c 50 "$BASE/index.html" >/dev/null 2>&1 && kill -0 $SERVER_PID 2>/dev/null \
            && ok "charge ApacheBench 2000 req / 50 concurrentes" || ko "charge ApacheBench" "le serveur a échoué"
    fi
    kill -TERM $SERVER_PID
    for _ in $(seq 1 50); do kill -0 $SERVER_PID 2>/dev/null || break; sleep 0.1; done
    if kill -0 $SERVER_PID 2>/dev/null; then ko "arrêt propre (SIGTERM)" "toujours en vie"; else
        wait $SERVER_PID; expect "arrêt propre (SIGTERM, code 0)" "0" "$?"; fi
    SERVER_PID=""
    grep -q "LITH est arrêté" server.log && ok "message d'arrêt dans le journal" || ko "message d'arrêt" "absent"

    "$LITH" start -d > /dev/null 2>&1
    wait_up "$PORT" && ok "mode démon : serveur joignable" || ko "mode démon" "injoignable"
    [ -f lith.pid ] && ok "fichier PID créé" || ko "fichier PID" "absent"
    "$LITH" stop >/dev/null 2>&1; expect "lith stop" "0" "$?"
    [ ! -f lith.pid ] && ok "fichier PID supprimé" || ko "fichier PID" "toujours présent"
fi

echo
echo "Résultat : $PASS réussi(s), $FAIL échec(s)"
[ $FAIL -eq 0 ] || { echo "--- journal du serveur ---"; tail -40 "$WORK/site/server.log" 2>/dev/null; exit 1; }
