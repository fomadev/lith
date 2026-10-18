# Contributing to LITH

Thank you for your interest in **LITH**! To keep the code portable, safe and
consistent, every contribution follows the rules below.

By contributing to this repository, you agree that your code is governed by the
**Fordima Public License (FPL)** and integrated into the official FomaDev ecosystem.

---

## License and compliance

1. **Temporary forks only**: a fork is used to propose changes to the official
   repository. Permanent, independent forks intended to bypass the FomaDev
   architecture or license are not allowed.
2. **Commercial restrictions**: modified versions of LITH, its code or its
   derivatives may not be distributed, resold or embedded in competing
   commercial software without prior commercial authorization.

---

## Setting up your environment

- A **C11** compiler: `gcc` or `clang` (Linux/macOS), **MinGW-w64** on
  Windows (MSYS2 or w64devkit).
- **GNU Make**.
- For integration tests: `bash` and `curl` (Git Bash works on Windows).
- Only for the optional HTTPS module (`make TLS=1`): [Rust](https://rustup.rs)
  (`cargo`) and `openssl` for the tests.

```bash
git clone https://github.com/fomadev/lith.git
cd lith
git checkout -b feature/my-feature   # or fix/my-fix
make
make test
make integration
```

---

## Code layout

| File | Role |
|------|------|
| `src/main.c` | Command line (`start`, `stop`, `init`, `check`, `health`) |
| `src/server.c` | Network listening (HTTP and HTTPS), thread pool, connection lifecycle |
| `src/tls.c` | Connection I/O (`LithStream`): plain socket, or through the TLS module |
| `src/http_parser.c` | HTTP parsing, MIME types, dates, paths, Range |
| `src/router.c` | Request routing |
| `src/static.c` | Static files (304, Range, gzip, cache, directory listings) |
| `src/response.c` | Building responses and error pages |
| `src/cache.c` | LRU in-memory cache |
| `src/crypto.c` | SHA-1, Base64, CRC-32, gzip compression |
| `src/security.c` | Basic authentication, rate limiting |
| `src/proxy.c` | Reverse proxy, load balancing, health checks |
| `src/websocket.c` | WebSocket and live reload |
| `src/metrics.c` | `/__lith/status` and `/__lith/metrics` statistics |
| `src/config.c` | Reading and validating `lith.conf` |
| `src/platform.c` | Everything that depends on the operating system |
| `src/logger.c`, `src/strbuf.c`, `src/daemon.c` | Logs, growable strings, daemon mode |
| `tls/` | Optional HTTPS module in Rust (rustls), C API in `tls/lith_tls.h` |

Each `src/x.c` file has its header in `include/`. A new file placed in `src/`
is compiled automatically by the `Makefile`.

---

## Technical rules

### Portability
LITH must compile **without warnings** (`-Wall -Wextra`) and behave the same
way on Linux, macOS and Windows. System-specific code goes into
`src/platform.c`, guarded by `#ifdef _WIN32`. Elsewhere, use the abstractions
(`socket_t`, `lith_stat`, `lith_fopen`, `lith_send_all`...).

### Zero dependencies
No external library is added to the LITH core. A feature that would require
one must be **optional** and enabled at build time (like HTTPS with `make TLS=1`).

### Memory safety
- Bounded functions (`snprintf`, `memcpy` with checked sizes), never `strcpy`/`sprintf`.
- Any data coming from the network is considered hostile: validate lengths.
- Every `malloc` has its `free`, including on error paths.
- Check with `make DEBUG=1 && make test DEBUG=1` (AddressSanitizer/UBSan on Linux/macOS).

### Tests
Every new feature comes with tests:
- pure function (parsing, computation) → `tests/unit_tests.c`;
- network behavior → `tests/integration.sh`.

### Commit messages
A short sentence in the imperative mood describing the change:

```text
Add If-Range support for partial requests
```

and not: `fix`.

---

## Reporting a problem

Open a GitHub *Issue* with:
- operating system and compiler;
- steps to reproduce;
- expected behavior and observed behavior;
- LITH logs (run with `LOG_LEVEL=debug` if possible) and the output of `lith check`.

For a security vulnerability, contact FomaDev privately before any public disclosure.

---

## Questions

For any question about the architecture, the license or a commercial
collaboration, contact Fordi (FomaDev) directly.
