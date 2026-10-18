# LITH — Light HTTP Server

**LITH** is a lightweight, fast and secure web server written in **standard C11**,
with **no external dependencies**. A single small executable (≈ 300 KB) is all
you need to publish a website, develop locally with automatic browser reload,
or put an application (Node.js, Python, Go...) behind a reverse proxy.

It works the same way on **Windows**, **Linux** and **macOS**.

```text
lith init my-site
cd my-site
lith start
→ http://localhost:8090
```

> **New here?** The full step-by-step explanation, starting from scratch, is in
> **[DOCUMENTATION.md](DOCUMENTATION.md)**. This README is only a summary.

---

## Why LITH?

| | |
|---|---|
| **Tiny** | ≈ 300 KB, a few MB of RAM, ≈ 340 KB Docker image |
| **Zero dependencies** | No OpenSSL, no zlib, no runtime: only the system C library |
| **Optional HTTPS** | `make TLS=1`: TLS 1.2/1.3 through a small Rust module (rustls), HTTP → HTTPS redirect, HSTS |
| **Fast** | Thread pool, keep-alive, in-memory cache, gzip, `sendfile()` |
| **Secure by default** | Path traversal protection, hidden files blocked, security headers, anti-slowloris |
| **Complete** | Reverse proxy, load balancing, WebSocket, live reload, Prometheus metrics |
| **Tested** | Unit + integration tests, under sanitizers, CI on 3 operating systems + Docker |

## Features in 1.0.0

- **Static files**: in-memory LRU cache, **gzip** compression, `ETag`/`304`,
  **Range** requests (video), directory listing, **SPA** mode, custom error pages.
- **Solid HTTP/1.1**: keep-alive, pipelining, `100-continue`, size limits, precise error codes.
- **Security**: per-IP rate limiting, per-path **Basic** authentication,
  `Content-Security-Policy`, **CORS**, `.env`/`.git` blocked.
- **Reverse proxy**: `PROXY=/api http://127.0.0.1:3000`, several backends in
  **round-robin**, automatic failover and **health checks**.
- **Development**: `lith start --dev` reloads the browser on every change (WebSocket).
- **Monitoring**: `/__lith/health`, `/__lith/status` (JSON), `/__lith/metrics` (Prometheus).
- **Operations**: daemon mode + `lith stop`, access log, graceful shutdown, Docker.
- **HTTPS (`make TLS=1` option)**: `TLS_PORT`, PEM certificates (Let's Encrypt), `TLS_REDIRECT`, `HSTS`,
  `X-Forwarded-Proto: https` sent to backends. Without the option, the core stays 100% C with no dependencies.

---

## Installation

### Download an executable
Get the archive for your system from the
[GitHub Releases](https://github.com/fomadev/lith/releases), extract it, and
run `bin/lith` (or `bin\lith.exe`).

### Build it yourself
You need a C11 compiler (`gcc`/`clang`, or **MinGW-w64** on Windows) and `make`.

```bash
git clone https://github.com/fomadev/lith.git
cd lith
make            # builds bin/lith (or bin/lith.exe)
make test       # unit tests
```

With HTTPS (you also need [Rust](https://rustup.rs); on Windows with
MinGW: `rustup target add x86_64-pc-windows-gnu`):

```bash
make TLS=1      # adds the tls/ module (rustls)
make TLS=1 test
```

### Docker

Ready-made image on [Docker Hub](https://hub.docker.com/r/fomadev0/lith)
(`linux/amd64` and `linux/arm64`, ≈ 340 KB):

```bash
docker run -p 8090:8090 -v "$PWD/public:/srv/public:ro" fomadev0/lith
```

Or build it yourself: `docker build -t lith .`

---

## Usage

```bash
lith start                  # start with lith.conf (port 8090 by default)
lith start 8080             # another port
lith start --dev            # automatic browser reload
lith start -r ./dist        # serve another folder
lith start -d               # in the background (Linux/macOS), then: lith stop
lith check                  # check the configuration
lith health                 # test a running server
lith help                   # all commands
```

## Configuration at a glance

Everything is set in `lith.conf` (created by `lith init`), in `KEY=VALUE` format:

```ini
PORT=8090
PUBLIC_DIR=public
GZIP=1
USE_CACHE=1
RATE_LIMIT=50                      # 50 requests/s per IP
AUTH=/admin admin:password         # protected area
PROXY=/api http://127.0.0.1:3000   # send /api to your application
SPA_FALLBACK=1                     # React, Vue, Angular...

# HTTPS (executable built with make TLS=1)
TLS_PORT=443
TLS_CERT=fullchain.pem
TLS_KEY=privkey.pem
TLS_REDIRECT=1                     # http:// -> https://
HSTS=31536000
```

The full list of settings (more than 35) is described in
[chapter 5 of the documentation](DOCUMENTATION.md#chapter-5--the-lithconf-configuration-file).

---

## Repository layout

```text
include/   C headers (one per module)
src/       source code: server, HTTP, files, proxy, WebSocket...
tls/       optional HTTPS module in Rust (rustls), built with make TLS=1
tests/     unit tests (C) and integration tests (bash + curl)
public/    sample site served by default
docker/    Docker image configuration
lith.conf  commented configuration
```

## Documentation

| Document | Contents |
|----------|----------|
| **[DOCUMENTATION.md](DOCUMENTATION.md)** | The complete guide, from chapter 0 ("what is a web server?") to production, internal architecture and roadmap |
| [CHANGELOG.md](CHANGELOG.md) | What changed in each version |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute to the code |

## License

Distributed under the [Fordima Public License (FPL)](LICENSE). Using the
compiled executables is free, including in companies. Commercial
redistribution, embedding the source code in a competing proprietary tool and
permanent independent forks require a commercial authorization.
See the [LICENSE](LICENSE) file for the exact terms.
