# Changelog

All notable changes to LITH are listed here.
The project follows [semantic versioning](https://semver.org/): `MAJOR.MINOR.PATCH`.

## [Unreleased]

### Added
- **Native HTTPS, as a build option**: `make TLS=1`. Encryption is handled by a
  small Rust module (`tls/`, **rustls** library, *ring* crypto provider) built
  as a static library and called through a minimal C API (`tls/lith_tls.h`).
  Without `TLS=1`, the executable stays 100% C with no dependencies, and its
  behavior is unchanged.
- New configuration keys: `TLS_PORT` (HTTPS port, in addition to `PORT`),
  `TLS_CERT` / `TLS_KEY` (PEM certificate and key), `TLS_REDIRECT`
  (`301`/`308` redirect from HTTP to HTTPS, except `/__lith/health` and
  `/.well-known/acme-challenge/`) and `HSTS` (`Strict-Transport-Security`).
- TLS 1.2 and 1.3, ALPN `http/1.1`, session resumption; every feature
  (files, gzip, Range, keep-alive, proxy, live reload over `wss://`,
  statistics) works over HTTPS, with the same anti-slowloris timeouts,
  handshake included.
- `lith check` actually loads the certificate and key and explains errors;
  an executable built without HTTPS rejects `TLS_PORT` instead of ignoring it.
- HTTPS integration tests (self-signed certificate generated on the fly, never
  stored in the repository), unit tests for the configuration and the proxy;
  CI builds and tests `make TLS=1` on Linux (with sanitizers) and Windows.
- New integration tests: suffix Range, If-Modified-Since, CORS preflight,
  live reload frame after a file change, `RATE_LIMIT` and `Retry-After`,
  `STATUS_AUTH`, `CSP`, `CACHE_CONTROL`, `LOG_FILE`.
- `make distclean` also removes the Rust build.

### Changed
- Reverse proxy: `X-Forwarded-Proto` is `https` when the visitor came in over
  HTTPS (it was always `http` before).
- Internal: all I/O of a client connection goes through `LithStream`
  (`src/tls.c`), which picks between the raw socket and the TLS session.
- Switching between `make` and `make TLS=1` relinks without `make clean`;
  TLS objects are stored in `build/tls/`.
- Documentation (README, DOCUMENTATION, CONTRIBUTING, CHANGELOG) translated to English.

## [1.0.0] - 2026-10-18

First stable release. The engine was fully restructured into independent
modules, tested on Windows, Linux and macOS, still with no external
dependencies.

### HTTP server
- Strict HTTP/1.0 and HTTP/1.1 parser: methods, headers (64 max, 16 KB max),
  `Host` required in 1.1, `%XX` decoding, rejection of ambiguous requests
  (`Content-Length` + `Transfer-Encoding`, duplicated headers, line folding).
- Keep-alive, pipelining, `Expect: 100-continue`, request bodies up to `MAX_BODY_SIZE`.
- Precise error codes: 400, 401, 403, 404, 405, 408, 411, 413, 414, 416, 429, 431, 501, 502, 503, 504, 505.
- Automatically sized thread pool, bounded queue (`MAX_PENDING`),
  idle connections released under heavy load.
- Graceful shutdown on Ctrl+C / `SIGTERM` / `docker stop`.
- IPv4 and IPv6 listening (`HOST=::`).

### Static files
- Chunked sending (files of any size), `sendfile()` on Linux.
- `ETag` / `Last-Modified` and `304 Not Modified` responses.
- `Range` partial requests (video/audio playback, download resume).
- Built-in gzip compression (custom implementation, no zlib).
- LRU in-memory cache with automatic reload of modified files.
- 40 MIME types, accented (UTF-8) file names, including on Windows.
- Directory listing (`AUTO_INDEX`), single-page application mode
  (`SPA_FALLBACK`), custom error pages (`public/404.html`...).

### Security
- Path traversal protection (including encoded forms and Windows pitfalls).
- Hidden files (`.env`, `.git`...) invisible by default.
- Security headers, configurable `Content-Security-Policy` and CORS.
- Per-IP rate limiting (`RATE_LIMIT`, 429 response).
- Per-path HTTP Basic authentication (`AUTH`).
- Anti-slowloris protection (strict read timeouts).

### Advanced features
- Reverse proxy (`PROXY`) with round-robin load balancing,
  automatic failover and periodic backend health checks.
- WebSocket (RFC 6455) and automatic browser reload (`--dev`).
- Monitoring: `/__lith/health`, `/__lith/status` (JSON), `/__lith/metrics` (Prometheus).
- Access log, log levels, log file.

### Tooling
- New command line: `start`, `stop`, `init`, `check`, `health`, `version`, `help`.
- Daemon mode with PID file (Linux/macOS).
- "scratch" Docker image of about 330 KB with health check.
- Unit tests (`make test`) and integration tests (`make integration`),
  continuous integration on Linux/macOS/Windows/Docker with sanitizers.
- Complete beginner-friendly documentation: `DOCUMENTATION.md`.
