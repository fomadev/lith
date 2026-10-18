# LITH 1.0.0 Documentation

> The complete LITH guide, written to be understood **even if you have never
> installed a web server**. Every concept is explained before it is used.
> Experienced developers can jump straight to the chapters they need.

---

## Contents

- [Chapter 0 — Introduction](#chapter-0--introduction)
- [Chapter 1 — Web basics](#chapter-1--web-basics)
- [Chapter 2 — Installing LITH](#chapter-2--installing-lith)
- [Chapter 3 — Your first site in 5 minutes](#chapter-3--your-first-site-in-5-minutes)
- [Chapter 4 — The command line](#chapter-4--the-command-line)
- [Chapter 5 — The lith.conf configuration file](#chapter-5--the-lithconf-configuration-file)
- [Chapter 6 — Serving a website](#chapter-6--serving-a-website)
- [Chapter 7 — Performance](#chapter-7--performance)
- [Chapter 8 — Security](#chapter-8--security)
- [Chapter 9 — Development mode (live reload)](#chapter-9--development-mode-live-reload)
- [Chapter 10 — Reverse proxy and load balancing](#chapter-10--reverse-proxy-and-load-balancing)
- [Chapter 11 — Monitoring and logs](#chapter-11--monitoring-and-logs)
- [Chapter 12 — Going to production](#chapter-12--going-to-production)
- [Chapter 13 — How LITH works inside](#chapter-13--how-lith-works-inside)
- [Chapter 14 — Building, testing and contributing](#chapter-14--building-testing-and-contributing)
- [Chapter 15 — Troubleshooting and FAQ](#chapter-15--troubleshooting-and-faq)
- [Chapter 16 — Roadmap and technical choices (including Rust)](#chapter-16--roadmap-and-technical-choices-including-rust)
- [Chapter 17 — Glossary](#chapter-17--glossary)
- [Appendix A — Cheat sheet](#appendix-a--cheat-sheet)
- [Appendix B — HTTP status codes used by LITH](#appendix-b--http-status-codes-used-by-lith)

---

## Chapter 0 — Introduction

### 0.1 What is LITH?

LITH (short for *Light HTTP Server*) is a **web server**: a program that
waits for a browser to ask for a page, then sends it.

When you type an address in your browser, somewhere a web server receives that
request and answers with the content of the page. LITH plays that role, on your
own computer or on a server on the Internet.

What makes it special:

- **It is tiny**: a single executable file of about 300 KB, using a few
  megabytes of memory.
- **It needs nothing else**: no Node.js, no Python, no library to install.
  Copy it, run it, it works.
- **It runs everywhere**: Windows, Linux, macOS, Raspberry Pi, Docker.
- **It is secure by default**: the important protections are active without
  any configuration.
- **It is written in C11**, a very fast and very portable language.

### 0.2 What can it be used for?

| Need | What LITH does for you |
|------|------------------------|
| See my HTML/CSS/JS site in the browser while I code it | `lith start --dev`: the page reloads by itself on every change |
| Publish a showcase site, a portfolio, documentation | Serves files fast, compressed, with caching |
| Host a React / Vue / Angular application | `SPA_FALLBACK` mode so the application routes work |
| Put my Node.js / Python API behind a single address | Reverse proxy: `/api` goes to your application |
| Spread traffic over several copies of my application | Load balancing with failure detection |
| Protect a page with a password | `AUTH=/admin user:password` |
| Monitor my server | Statistics pages, Prometheus/Grafana format |
| Deploy to the cloud | ≈ 330 KB Docker image |

### 0.3 What LITH does not do (yet)

Better to know it from the start:

- **HTTPS** (the padlock in the browser) is a **build option**: the standard
  executable does not include it (it stays free of any dependency); built with
  `make TLS=1`, LITH serves `https://` directly. Another solution: put LITH
  behind a tool that handles HTTPS (Caddy, nginx, Cloudflare...). Both methods
  are explained step by step in [chapter 12.4](#124-adding-https).
- LITH **does not obtain** certificates by itself: you provide them (Let's
  Encrypt with `certbot`, for example), or you let Caddy take care of it.
- LITH **does not run server-side code** (PHP, Python...) itself. However, it
  can forward requests to an application that does (chapter 10).
- It speaks HTTP/1.0 and HTTP/1.1 (not HTTP/2 or HTTP/3 yet). That is more than
  enough for the vast majority of websites.

### 0.4 How to read this guide

- If you are a beginner: read chapters **1 to 6 in order**. You will have a
  site running locally by the end of chapter 3.
- If you want to put a site online: chapters **8 and 12**.
- If you are a curious developer: chapters **13, 14 and 16**.

**Conventions**:

- A block like this one is a command to type in a **terminal**:

  ```bash
  lith start
  ```

- The *terminal* (or *command prompt*) is the window where you type commands:
  **PowerShell** or **Command Prompt** on Windows, **Terminal** on macOS and Linux.
- `<something>` means "replace with your own value".
- LITH's own messages (logs, `lith check`, `lith init`) are currently in
  French. They are quoted exactly as the program prints them, with their
  meaning explained in the text.

---

## Chapter 1 — Web basics

This chapter explains, without unnecessary jargon, what happens when you open
a web page. If you already know HTTP, skip to chapter 2.

### 1.1 Client and server

The web works like a restaurant:

- the **client** (your browser: Chrome, Firefox, Edge, Safari) places an
  **order**: "I would like the home page";
- the **server** (LITH) fetches the dish from the kitchen (the file on disk)
  and **serves** it to the client.

```text
   Browser (client)                            LITH (server)
  ┌──────────────────┐   1. request            ┌──────────────────┐
  │                  │ ──────────────────────▶ │                  │
  │  http://.../     │   "GET /index.html"     │  reads the file   │
  │                  │                         │  public/index.html│
  │                  │ ◀────────────────────── │                  │
  └──────────────────┘   2. response           └──────────────────┘
                          "200 OK" + the HTML
```

### 1.2 HTTP: the common language

The browser and the server talk to each other in a language called **HTTP**
(*HyperText Transfer Protocol*). It is very simple text. Here is a real
request sent by a browser:

```http
GET /contact.html HTTP/1.1
Host: localhost:8090
User-Agent: Mozilla/5.0 ...
Accept-Encoding: gzip
```

- `GET` is the **method**: "give me". (There are others: `POST` to send a
  form, `HEAD` to ask only for the information...)
- `/contact.html` is the **path** of the requested resource.
- `HTTP/1.1` is the protocol version.
- The following lines are **headers**: extra information ("I accept gzip
  compression", "I am Firefox"...).

And LITH's response:

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 1234

<!DOCTYPE html>
<html>...
```

- `200 OK` is the **status code**: here, "all good".
- `Content-Type` tells the browser what it is receiving (an HTML page, an image...).
- After an empty line comes the **body**: the content of the file.

### 1.3 Status codes

The three-digit code summarizes the result:

| Family | Meaning | Common examples |
|--------|---------|-----------------|
| 2xx | Success | `200 OK`, `206 Partial Content` (a piece of a video) |
| 3xx | Redirect or "nothing new" | `301` (the address has changed), `304 Not Modified` ("you already have the right version") |
| 4xx | Client error | `404 Not Found` (does not exist), `403 Forbidden`, `401` (password required), `429` (too many requests) |
| 5xx | Server error | `500` (internal problem), `502` (the application behind the proxy does not answer) |

The full list used by LITH is in [appendix B](#appendix-b--http-status-codes-used-by-lith).

### 1.4 URL, address, port and localhost

A **URL** such as `http://localhost:8090/blog/article.html` breaks down like this:

| Part | Example | Meaning |
|------|---------|---------|
| Protocol | `http://` | The language used |
| Host | `localhost` | The machine to reach. `localhost` = "my own computer" |
| Port | `8090` | The "door" of the machine where LITH listens |
| Path | `/blog/article.html` | The requested file |

**The port** is like an apartment number in a building: the address designates
the building (the machine), the port designates the door (the program). The
same computer can run several servers, each on its own port. LITH uses port
**8090** by default. Port **80** is the one of the regular web (when nothing is
specified in the address), **443** is the one of HTTPS.

**`localhost`** (or `127.0.0.1`) always designates your own machine. A site
opened at `http://localhost:8090` is only visible from your computer... unless
you give your network address to other devices (see 6.9).

### 1.5 Static or dynamic site?

- A **static site** is made of ready-to-use files (HTML, CSS, JavaScript,
  images). The server sends them as they are. That is LITH's main job, and it
  is extremely fast.
- A **dynamic site** builds pages on demand with a program (PHP, Node.js,
  Python...). LITH does not build these pages itself, but it can sit in front
  of your application and forward requests to it: that is the **reverse
  proxy** (chapter 10).

> **Tip:** Modern React, Vue, Svelte or Angular applications, once "built"
> (`npm run build`), are **static sites**: LITH can serve them directly.

---

## Chapter 2 — Installing LITH

Two options: download a ready-to-use executable (the simplest), or build it
yourself.

### 2.1 Download an executable (recommended for beginners)

1. Go to the **Releases** page: <https://github.com/fomadev/lith/releases>
2. Download the archive for your system:
   - Windows: `lith-v1.0.0-windows-x64.zip`
   - Linux: `lith-v1.0.0-linux-x64.tar.gz`
   - macOS: `lith-v1.0.0-macos.tar.gz`
3. Extract it. You get:

   ```text
   lith-v1.0.0-.../
   ├── bin/lith(.exe)     ← the server
   ├── public/index.html  ← a sample site
   ├── lith.conf          ← the commented configuration
   ├── README.md, DOCUMENTATION.md, CHANGELOG.md, LICENSE
   ```

4. Check that everything works. Open a terminal **in this folder** and type:

   ```bash
   bin/lith version        # Linux / macOS
   bin\lith.exe version    # Windows
   ```

   You should see `LITH 1.0.0`.

> **macOS:** on first launch, macOS may block a downloaded program. Allow it
> in *System Settings → Privacy & Security*, or type
> `xattr -d com.apple.quarantine bin/lith`.

### 2.2 Making the `lith` command available everywhere

To simply type `lith` from any folder, the executable must be in the **PATH**
(the list of folders where the system looks for commands).

**Windows**:
1. Create a folder, for example `C:\lith\bin`, and copy `lith.exe` into it.
2. Start menu → type "environment variables" → *Edit environment variables
   for your account*.
3. Select `Path` → *Edit* → *New* → `C:\lith\bin` → OK.
4. **Close and reopen** the terminal, then type `lith version`.

**Linux / macOS**:

```bash
sudo cp bin/lith /usr/local/bin/
lith version
```

### 2.3 Building LITH yourself

Building means turning the source code (`.c` files) into an executable
program. You need two tools: a **C compiler** and **make**.

**Linux (Debian, Ubuntu...)**:

```bash
sudo apt install build-essential git
git clone https://github.com/fomadev/lith.git
cd lith
make
sudo make install      # optional: copies lith into /usr/local/bin
```

**macOS**:

```bash
xcode-select --install          # installs the compiler (clang) and make
git clone https://github.com/fomadev/lith.git
cd lith
make
```

**Windows**: install a **MinGW-w64** environment, either:
- [w64devkit](https://github.com/skeeto/w64devkit): an archive to extract,
  nothing to install;
- [MSYS2](https://www.msys2.org/): then `pacman -S mingw-w64-x86_64-gcc make`
  in the "MSYS2 MINGW64" terminal.

Then, in that terminal:

```bash
git clone https://github.com/fomadev/lith.git
cd lith
make
```

The executable is created in `bin/lith` (or `bin\lith.exe`). To check that
everything is correct:

```bash
make test
```

You should see `168 vérifications, 0 échec(s)` ("168 checks, 0 failures"; the
number may grow with new versions).

**With HTTPS (optional)**: you also need [Rust](https://rustup.rs) (`cargo`),
which builds the small encryption module in the `tls/` folder (based on the
rustls library), then:

```bash
make TLS=1
make TLS=1 test
```

On Windows with MinGW, add once and for all the Rust target compatible with
gcc: `rustup target add x86_64-pc-windows-gnu` (with MSYS2, installing
`mingw-w64-x86_64-rust` is enough). The first build downloads the Rust
dependencies (versions pinned by `tls/Cargo.lock`) and takes one or two
minutes; the next ones are instant. The executable grows from about 300 KB to
about 1.5 MB. Without `TLS=1`, none of this is involved.

### 2.4 With Docker

If you know Docker, no installation is needed:

```bash
docker build -t lith .
docker run -p 8090:8090 lith
```

Details are in [chapter 12.5](#125-deploying-with-docker).

---

## Chapter 3 — Your first site in 5 minutes

### 3.1 Create the project

In a terminal, go to where you want to create your site (for example your
Documents folder), then:

```bash
lith init my-site
```

LITH creates:

```text
my-site/
├── lith.conf          ← the configuration (every setting, commented)
└── public/
    └── index.html     ← your home page
```

The **`public`** folder is the root of your site: everything you put in it
becomes reachable from the browser.

### 3.2 Start the server

```bash
cd my-site
lith start
```

You will see something like this ("ready", "press Ctrl+C to stop"):

```text
[2026-10-18 10:00:00] INFO  LITH 1.0.0 prêt : http://localhost:8090  (dossier 'public', 32 threads, cache on, gzip on)
[2026-10-18 10:00:00] INFO  Appuyez sur Ctrl+C pour arrêter.
```

### 3.3 Open the site

Open your browser at **<http://localhost:8090>**.
The "Ça marche !" ("It works!") page is displayed.

In the terminal, a line appears for each request:

```text
[2026-10-18 10:00:05] 127.0.0.1 "GET / HTTP/1.1" 200 772 0ms "Mozilla/5.0 ..."
```

It reads: who asked (`127.0.0.1` = you), what (`GET /`), the result (`200`),
the size sent (772 bytes) and the processing time.

### 3.4 Edit the site

1. Open `public/index.html` in your editor (VS Code, Notepad++...).
2. Change the text, save.
3. Reload the page in the browser (F5): the change appears.

Add more files in `public/`: `public/contact.html` will be reachable at
`http://localhost:8090/contact.html`, `public/images/logo.png` at
`http://localhost:8090/images/logo.png`.

> **Tip:** To stop pressing F5, run `lith start --dev` instead (chapter 9):
> the browser reloads by itself.

### 3.5 Stop the server

In the terminal, press **Ctrl + C**. LITH finishes the requests in progress,
then stops ("shutting down", "LITH has stopped. See you soon!"):

```text
[...] INFO  Arrêt en cours : fin des requêtes en cours...
[...] INFO  LITH est arrêté. À bientôt !
```

### 3.6 If something goes wrong

| Message | Cause | Solution |
|---------|-------|----------|
| `Le port 8090 est déjà utilisé` (port already in use) | Another program (or another LITH) uses this port | `lith start 8091`, or stop the other program |
| `Le dossier public 'public' est introuvable` (public folder not found) | You are not in the right folder | `cd my-site`, or `lith start -r path/to/the/site` |
| `lith: command not found` | `lith` is not in the PATH | See 2.2, or use the full path (`./bin/lith`) |
| The page shows `404 Not Found` | The file does not exist in `public/` | Check the exact name (uppercase, extension) |

More cases are covered in [chapter 15](#chapter-15--troubleshooting-and-faq).

---

## Chapter 4 — The command line

Every command is written `lith <command> [options]`.

### 4.1 Commands

| Command | Role |
|---------|------|
| `lith start [PORT]` | Starts the server. The port is optional (otherwise: `lith.conf`, otherwise 8090) |
| `lith stop` | Stops a LITH started in the background with `-d` (Linux/macOS) |
| `lith init [FOLDER]` | Creates `lith.conf` and `public/index.html` (without overwriting existing files) |
| `lith check` | Reads the configuration, reports errors and prints every value in use |
| `lith health [PORT]` | Checks that a LITH answers on this machine. Exit code 0 = OK, 1 = unreachable |
| `lith version` | Prints the version (`-v`, `--version` also work) |
| `lith help` | Prints the help (`-h`, `--help`) |

### 4.2 Options

| Option | Effect | Example |
|--------|--------|---------|
| `-c`, `--config FILE` | Uses a file other than `lith.conf` | `lith start -c prod.conf` |
| `-p`, `--port PORT` | Listening port | `lith start -p 3000` |
| `-H`, `--host ADDRESS` | Listening address | `lith start -H 127.0.0.1` |
| `-r`, `--root FOLDER` | Folder to serve | `lith start -r ./dist` |
| `-d`, `--daemon` | Background (Linux/macOS) | `lith start -d` |
| `--dev` | Development mode: automatic reload | `lith start --dev` |
| `-q`, `--quiet` | Does not print one line per request | `lith start -q` |

**Priority order** of settings: default value → `lith.conf` → command-line
options. An option typed in the terminal always wins.

### 4.3 Common examples

```bash
# Serve the build folder of a React application on port 3000
lith start -r ./build -p 3000

# Develop with automatic reload
lith start --dev

# Check a production configuration before using it
lith check -c /etc/lith/prod.conf

# Run in the background on a Linux server, then stop it
lith start -d
lith stop

# In a script: wait until the server is ready
until lith health; do sleep 1; done
```

### 4.4 Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success (or normal shutdown) |
| 1 | Error: invalid configuration, port in use, server unreachable... |
| 2 | Misspelled command or option |

---

## Chapter 5 — The lith.conf configuration file

### 5.1 Format

`lith.conf` is a plain text file:

```ini
# This is a comment (ignored line)
PORT=8090
PUBLIC_DIR=public
```

Rules:
- one setting per line, in `KEY=VALUE` format;
- lines starting with `#` (or `;`) are comments;
- spaces around `=` are ignored; quotes around the value are optional;
- keys are case-insensitive (`port` = `PORT`);
- **booleans**: `1`/`0`, `true`/`false`, `on`/`off`, `yes`/`no`, `oui`/`non`;
- **sizes**: a number of bytes, or with a unit: `512K`, `10M`, `1G`;
- `AUTH` and `PROXY` can be repeated on several lines.

**Every key is optional.** Without `lith.conf`, LITH starts with the default
values. An unknown key or an invalid value is reported with its line number,
and LITH refuses to start: you never end up with a half-applied configuration.

> **Advice:** After any change, run `lith check`: it shows the errors and the
> configuration actually in use.

### 5.2 Network

| Key | Default | Description |
|-----|---------|-------------|
| `HOST` | `0.0.0.0` | Listening address. `0.0.0.0` = every IPv4 interface (reachable from the local network). `127.0.0.1` = only from this machine. `::` = IPv4 and IPv6 |
| `PORT` | `8090` | Listening port (1 to 65535). Ports below 1024 require administrator rights on Linux |
| `THREADS` | `0` (auto) | Number of worker threads. Auto = 8 × number of cores, between 16 and 128 |
| `MAX_PENDING` | `1024` | Connections allowed to wait for a free thread. Beyond that: immediate `503` response |
| `KEEPALIVE_TIMEOUT` | `5` | Seconds an idle connection stays open for the next request. `0` disables keep-alive |
| `KEEPALIVE_MAX` | `100` | Maximum number of requests per connection |
| `REQUEST_TIMEOUT` | `10` | Seconds allowed to send the headers of a request (and between two chunks of the body). Protects against "slowloris" attacks |
| `MAX_BODY_SIZE` | `10M` | Maximum size of a request body (form, file upload to the proxy). Beyond that: `413` |

### 5.3 Files

| Key | Default | Description |
|-----|---------|-------------|
| `PUBLIC_DIR` | `public` | Folder containing the site. Relative path (to the folder where LITH is started) or absolute |
| `INDEX_FILE` | `index.html` | File served when a folder is requested (`/` or `/blog/`) |
| `AUTO_INDEX` | `0` | `1` = list the files of a folder that has no `index.html` |
| `SPA_FALLBACK` | `0` | `1` = for single-page applications: an unknown address **without extension** (`/profile/42`) returns `index.html` |
| `HIDE_DOTFILES` | `1` | `1` = files and folders starting with a dot (`.env`, `.git/`) cannot be found (`404`). Only `/.well-known/` stays reachable |
| `CACHE_CONTROL` | `no-cache` | Value of the `Cache-Control` header of files (see 7.4). Empty = not sent |

### 5.4 Performance

| Key | Default | Description |
|-----|---------|-------------|
| `USE_CACHE` | `1` | Keep files in RAM. A file modified on disk is reloaded automatically |
| `CACHE_MAX_SIZE` | `64M` | Maximum memory used by the cache. The least requested files are evicted first |
| `CACHE_FILE_MAX` | `2M` | Maximum size of a file to enter the cache. Larger ones are read directly from disk |
| `GZIP` | `1` | Compress text files (HTML, CSS, JS, JSON, SVG...) for browsers that accept it |

### 5.5 Security

| Key | Default | Description |
|-----|---------|-------------|
| `SECURITY_HEADERS` | `1` | Adds `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN` and `Referrer-Policy` (see 8.2) |
| `CSP` | *(empty)* | Value of the `Content-Security-Policy` header. E.g.: `default-src 'self'` |
| `CORS_ORIGIN` | *(empty)* | Allows JavaScript calls from another site. `*` = everyone, or a specific origin: `https://mysite.com` |
| `RATE_LIMIT` | `0` | Number of requests per second allowed **per IP address**. `0` = no limit. Beyond that: `429` |
| `RATE_BURST` | `2 × RATE_LIMIT` | Reserve of requests allowed at once (burst). If missing or lower than `RATE_LIMIT`, it is `2 × RATE_LIMIT` |
| `AUTH` | — | Protects a path with a password: `AUTH=/admin user:password`. Repeatable (16 max) |

### 5.6 Reverse proxy

| Key | Default | Description |
|-----|---------|-------------|
| `PROXY` | — | `PROXY=/prefix http://host:port[, http://host2:port2...]`. Repeatable (16 routes, 8 backends per route) |
| `PROXY_TIMEOUT` | `30` | Maximum seconds to wait for a backend response. Beyond that: `504` |
| `HEALTH_CHECK_INTERVAL` | `5` | Seconds between two backend checks. `0` = disabled |

### 5.7 Development and monitoring

| Key | Default | Description |
|-----|---------|-------------|
| `LIVE_RELOAD` | `0` | Reloads the browser on every file change (enabled by `--dev`) |
| `STATUS` | `1` | Enables `/__lith/status` and `/__lith/metrics` |
| `STATUS_AUTH` | *(empty)* | `user:password`. Without this key, statistics are only visible from the machine itself |

### 5.8 Logs and process

| Key | Default | Description |
|-----|---------|-------------|
| `LOG_LEVEL` | `info` | Level of detail: `debug`, `info`, `warn`, `error` |
| `LOG_FILE` | *(empty)* | File to write logs to. Empty = in the terminal (in daemon mode: `lith.log`) |
| `ACCESS_LOG` | `1` | One line per request (can be disabled with `-q`) |
| `PID_FILE` | `lith.pid` | File holding the process number in daemon mode (used by `lith stop`) |

### 5.9 Complete configuration examples

**Local development**

```ini
HOST=127.0.0.1
PORT=8090
LIVE_RELOAD=1
CACHE_CONTROL=no-store
```

**React/Vue application in production**

```ini
PORT=8090
PUBLIC_DIR=/srv/my-app/dist
SPA_FALLBACK=1
GZIP=1
USE_CACHE=1
CACHE_MAX_SIZE=128M
RATE_LIMIT=50
CSP=default-src 'self'; img-src 'self' data:
```

**Site + Node.js API**

```ini
PORT=8090
PUBLIC_DIR=public
PROXY=/api http://127.0.0.1:3000
AUTH=/admin admin:a-long-password
```

**Public site with direct HTTPS** (executable built with `make TLS=1`, see 5.10)

```ini
PORT=80
TLS_PORT=443
TLS_CERT=/etc/letsencrypt/live/mysite.com/fullchain.pem
TLS_KEY=/etc/letsencrypt/live/mysite.com/privkey.pem
TLS_REDIRECT=1
HSTS=31536000
```

### 5.10 HTTPS (`make TLS=1`)

These keys only apply to an executable built with `make TLS=1` (chapter 2.3).
With the standard executable, `TLS_PORT` is **rejected** with a clear message:
LITH never starts in plain text when you expected encryption.

| Key | Default | Description |
|-----|---------|-------------|
| `TLS_PORT` | `0` | HTTPS port (`443` in production, `8443` to try it out). `0` = HTTPS disabled. The HTTP port (`PORT`) stays open in parallel and must be different |
| `TLS_CERT` | *(empty)* | Certificate in PEM format, **full chain** (your certificate followed by the intermediate certificates: `fullchain.pem` with Let's Encrypt) |
| `TLS_KEY` | *(empty)* | Private key in PEM format (PKCS#8, RSA or EC: `privkey.pem` with Let's Encrypt). Protect this file (`chmod 600`) |
| `TLS_REDIRECT` | `0` | `1` = the HTTP port answers with a redirect to `https://` (`301` for `GET`/`HEAD`, `308` for other methods, which keeps the body). These stay in plain HTTP: `/__lith/health` (probes, `lith health`) and `/.well-known/acme-challenge/` (Let's Encrypt validation) |
| `HSTS` | `0` | Duration in seconds of the `Strict-Transport-Security` header, sent over HTTPS only: the browser will use nothing but HTTPS for this site during that time. Start small (`300`), then `31536000` (one year) once you are confident |

Relative paths are relative to the folder where LITH is started (they are
converted to absolute paths in daemon mode). `lith check` actually loads the
certificate and the key and reports any problem (missing file, unreadable PEM,
key that does not match the certificate).

What LITH does over HTTPS: TLS 1.3 and TLS 1.2 only, modern cipher suites,
HTTP/1.1 announced through ALPN, session resumption. Everything else works the
same way: files, gzip, cache, `Range`, keep-alive, proxy (the backend receives
`X-Forwarded-Proto: https`), live reload over `wss://`, statistics. The
protections against slow clients also apply to the TLS handshake
(`REQUEST_TIMEOUT`).

To try it locally with a self-signed certificate (the browser will show a
warning, that is expected):

```bash
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -keyout key.pem -out cert.pem \
  -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
printf 'TLS_PORT=8443\nTLS_CERT=cert.pem\nTLS_KEY=key.pem\n' >> lith.conf
lith start          # then https://localhost:8443
```

> **Tip:** In Git Bash (Windows), prefix the `openssl` command with
> `MSYS_NO_PATHCONV=1` so that `/CN=localhost` is not taken for a path.

---

## Chapter 6 — Serving a website

### 6.1 From the URL path to the file

LITH appends the URL path to the `PUBLIC_DIR` folder:

| Requested URL | File sent |
|---------------|-----------|
| `/` | `public/index.html` |
| `/contact.html` | `public/contact.html` |
| `/css/style.css` | `public/css/style.css` |
| `/blog/` | `public/blog/index.html` |
| `/blog` | `301` redirect to `/blog/` (so that the page's relative links work) |

Special characters are decoded: `/my%20file.html` gives `public/my file.html`,
and accented names (`café.html`) work on every system, Windows included.

### 6.2 File types (MIME)

The browser needs to know what it receives. LITH states it in the
`Content-Type` header based on the extension: `.html`, `.css`, `.js`, `.mjs`,
`.json`, `.svg`, `.png`, `.jpg`, `.webp`, `.avif`, `.gif`, `.ico`, `.woff2`,
`.ttf`, `.mp4`, `.webm`, `.mp3`, `.ogg`, `.pdf`, `.wasm`, `.zip`,
`.webmanifest`, `.xml`, `.txt`, `.md`, `.csv`... (40 types). A file with an
unknown extension is sent as `application/octet-stream`: the browser will offer
to download it.

### 6.3 Custom error pages

By default, LITH shows a plain error page. To use your own, create in
`public/` a file named after the code: `404.html`, `403.html`, `500.html`...
LITH sends it automatically, with the right status code.

### 6.4 Single-page applications (React, Vue, Angular, Svelte...)

These applications manage their own addresses (`/profile/42`, `/settings`) in
the browser. But if the user reloads the page or opens a link directly, the
server receives `/profile/42`, which does not exist on disk.

With `SPA_FALLBACK=1`, LITH returns `index.html` for any unknown address
**without extension**; the application then takes over. Missing files with an
extension (`/logo.png`) still answer `404`, so real errors are not hidden.

```ini
PUBLIC_DIR=dist
SPA_FALLBACK=1
```

### 6.5 Listing the contents of a folder

With `AUTO_INDEX=1`, a folder without `index.html` shows the list of its files
(name, size, date), sorted, folders first. Handy for sharing files on a local
network. Hidden files do not appear while `HIDE_DOTFILES=1`.

### 6.6 Hidden files

Files starting with a dot often contain secrets: `.env` (passwords), `.git/`
(the whole code history), `.htpasswd`... With `HIDE_DOTFILES=1` (the default),
LITH answers `404` as if they did not exist. Exception: `/.well-known/`, used by
web standards (certificates, `security.txt`...).

### 6.7 Accepted methods

For static files, LITH accepts `GET`, `HEAD` and `OPTIONS`. Any other method
(`POST`, `PUT`, `DELETE`...) answers `405 Method Not Allowed`. These methods
are, however, forwarded as they are to applications behind the proxy
(chapter 10).

### 6.8 Reserved `/__lith/` addresses

Paths starting with `/__lith/` are reserved for LITH:

| Address | Role |
|---------|------|
| `/__lith/health` | Answers `OK`: used by availability probes (Docker, Kubernetes) |
| `/__lith/status` | Statistics in JSON (chapter 11) |
| `/__lith/metrics` | Statistics in Prometheus format (chapter 11) |
| `/__lith/livereload` | WebSocket channel of the automatic reload (chapter 9) |
| `/__lith/echo` | Diagnostics: with `POST`, returns what LITH received (size, type) |

### 6.9 Viewing the site from a phone or another computer

1. Keep `HOST=0.0.0.0` (the default value).
2. Find your computer's address on the network: `ipconfig` (Windows),
   `ip addr` (Linux), *Settings → Network* (macOS). For example `192.168.1.20`.
3. On the phone, connected to the **same Wi-Fi**, open `http://192.168.1.20:8090`.
4. If nothing shows up, allow LITH in the **firewall** (Windows usually asks
   on first launch).

---

## Chapter 7 — Performance

LITH is fast without any tuning. This chapter explains why, and how to adjust it.

### 7.1 In-memory cache

Reading a file from disk takes time; reading it from RAM is almost instant.
With `USE_CACHE=1`, the first time a file is requested, LITH keeps it in
memory. The next times, it sends it straight from RAM.

- **Always up to date**: on every request, LITH checks the file's modification
  date. If it changed, it is reloaded. You can edit your site without
  restarting the server.
- **Bounded**: total memory never exceeds `CACHE_MAX_SIZE`. When full, the
  least recently requested files are evicted (*LRU* algorithm).
- Files larger than `CACHE_FILE_MAX` (videos, archives) bypass the cache: they
  are sent in chunks from disk, never fully loaded into memory. On Linux, LITH
  uses `sendfile()`, which moves data from disk to network without even
  copying it into the program.

### 7.2 Gzip compression

Text files compress very well: an HTML or JavaScript file often loses
**60 to 75%** of its size. With `GZIP=1`, if the browser accepts it
(`Accept-Encoding: gzip` header, sent by every modern browser), LITH sends the
compressed version.

- The compressed version is computed **only once** and then cached.
- Images, videos and archives (already compressed) are not compressed again.
- Neither are files under 256 bytes (no gain).
- Compression is **built into LITH** (no external library).

### 7.3 "You already have the right version": ETag and 304

Every file sent carries a **tag** (`ETag`) and its modification date
(`Last-Modified`). When the browser asks for the file again, it presents its
tag; if the file has not changed, LITH simply answers `304 Not Modified`,
without sending the content again. Result: pages that display instantly and
saved bandwidth.

### 7.4 Tuning the browser cache (`CACHE_CONTROL`)

The `Cache-Control` header tells the browser how long to keep a file without
asking again:

| Value | Effect | When to use it |
|-------|--------|----------------|
| `no-cache` (default) | The browser keeps the file but checks every time (very light `304` response) | The right default: always up to date, and fast |
| `no-store` | Nothing is kept | Development (enabled by `--dev`) |
| `public, max-age=3600` | Kept 1 hour without any check | Sites that rarely change |
| `public, max-age=31536000, immutable` | Kept 1 year | Files whose name changes with each version (`app.3f2a1c.js`, produced by Vite/Webpack) |

### 7.5 Partial requests (video, download resume)

When you skip ahead in a video, the browser only asks for a piece of the
file: `Range: bytes=1000000-2000000`. LITH answers `206 Partial Content` with
exactly that piece. This allows smooth video and audio playback, and resuming
interrupted downloads.

### 7.6 Keep-alive and threads

- **Keep-alive**: opening a network connection takes time. LITH keeps the
  connection open for a few seconds (`KEEPALIVE_TIMEOUT`) so that the browser
  can send the next requests over it (CSS, images...).
- **Threads**: LITH serves several visitors at the same time thanks to a group
  of "workers" (threads) created at startup. The automatic number almost always
  fits. If many visitors stay connected for a long time, increase `THREADS`.
- **Under heavy load**, when visitors are waiting for a free thread, LITH
  closes idle connections after each response to serve everyone fairly.

### 7.7 Orders of magnitude

On an ordinary laptop, LITH 1.0.0 serves several thousand requests per second
with less than 10 MB of memory. The automated tests send thousands of
concurrent requests without errors or resource leaks. The limiting factor is
usually the network, not LITH.

---

## Chapter 8 — Security

### 8.1 Automatic protections

Without any configuration, LITH:

| Protection | Against what |
|------------|--------------|
| Rejects `../` in paths, even encoded (`%2e%2e`) or with `\` | Reading files outside the site (`/../../etc/passwd`) → `403` |
| Rejects the null byte `%00` and Windows-specific pitfalls (`..` followed by spaces or dots, `:`) | Variants of the same attack |
| Hides `.env`, `.git/`... files | Leaking passwords or source code |
| Limits headers to 16 KB, the URL to 2048 characters, the body to `MAX_BODY_SIZE` | Memory exhaustion |
| Cuts off requests that are too slow (`REQUEST_TIMEOUT`) | The "slowloris" attack (tying up the server by sending a request byte by byte) |
| Rejects ambiguous requests (two `Content-Length`, `Content-Length` + `Transfer-Encoding`...) | "Request smuggling" (fooling a proxy placed in front) |
| Refuses connections beyond `MAX_PENDING` with `503` | Saturation |
| Adds security headers | See 8.2 |

### 8.2 Security headers

With `SECURITY_HEADERS=1` (the default), every response contains:

- `X-Content-Type-Options: nosniff`: the browser does not "guess" the type of
  a file (prevents running a text file as JavaScript);
- `X-Frame-Options: SAMEORIGIN`: your site cannot be displayed in a frame
  (`<iframe>`) on another site, which blocks *clickjacking*;
- `Referrer-Policy: strict-origin-when-cross-origin`: the full addresses of
  your pages are not passed on to other sites.

### 8.3 Content-Security-Policy (CSP)

The CSP tells the browser where the site is allowed to load scripts, styles
and images from. It is the best protection against XSS vulnerabilities
(JavaScript injection). It is not enabled by default because a badly tuned CSP
can break a site. To start:

```ini
CSP=default-src 'self'
```

Then add the sources you need, for example
`CSP=default-src 'self'; img-src 'self' data:; font-src https://fonts.gstatic.com`.
Open the browser console (F12): every block is reported there.

> **Warning:** Automatic reload (`--dev`) injects a small script into pages:
> do not enable a strict CSP together with `--dev`.

### 8.4 CORS: allowing other sites to call yours

For safety, a browser forbids JavaScript from `site-a.com` to read the
responses of `site-b.com`. If you want to allow it (for example for a public
API or JSON files):

```ini
CORS_ORIGIN=*                      # everyone
CORS_ORIGIN=https://site-a.com     # a single site
```

LITH then adds `Access-Control-Allow-Origin` and answers the prior check
requests (`OPTIONS`, called *preflight*).

### 8.5 Limiting the number of requests per visitor

```ini
RATE_LIMIT=20      # 20 requests per second per IP address
RATE_BURST=40      # with a reserve of 40 at once
```

This is the **token bucket** algorithm: each visitor has a bucket of
`RATE_BURST` tokens, refilled with `RATE_LIMIT` tokens per second. Each request
uses one token; empty bucket = `429 Too Many Requests` response with
`Retry-After: 1`. A normal page (HTML + CSS + images) goes through easily; a
bot sending hundreds of requests is slowed down.

> **Warning:** If LITH is behind another proxy (Caddy, nginx), every request
> seems to come from `127.0.0.1`: in that case, do the rate limiting in that proxy.

### 8.6 Protecting an area with a password

```ini
AUTH=/admin admin:a-long-password
AUTH=/private/reports team:another-password
```

The browser shows a login window for `/admin` and everything under it
(`/admin/stats.html`...), but not for `/administration` (the prefix must be
followed by `/` or the end of the path). The most specific rule wins. `AUTH`
also protects proxy routes.

> **Important:** Basic authentication sends the password with every request,
> merely encoded (not encrypted). On the Internet, use it **only behind
> HTTPS** (chapter 12.4). Since the password is written in plain text in
> `lith.conf`, protect that file (`chmod 600 lith.conf` on Linux).

### 8.7 HTTPS

HTTPS encrypts the exchanges (the browser padlock) and proves to the visitor
that they are really talking to your site. It is essential for a public site,
and mandatory as soon as there is a password (`AUTH`) or a form. Two options:

- **Built-in HTTPS**: build LITH with `make TLS=1`, then set `TLS_PORT`,
  `TLS_CERT` and `TLS_KEY` (chapter 5.10). Encryption is handled by a small
  module written in Rust with the **rustls** library, memory-safe and audited
  (chapter 16.4); the rest of LITH does not change.
- **HTTPS in front of LITH**: a tool such as **Caddy** obtains and renews
  certificates by itself, and forwards requests to LITH locally.

The step-by-step guide for both methods is in [chapter 12.4](#124-adding-https).

Once HTTPS is in place:

- `TLS_REDIRECT=1` automatically sends visitors from `http://` to `https://`;
- `HSTS` asks the browser never to come back in plain text. Warning: the
  browser remembers it for the whole duration given, even if you remove HTTPS
  afterwards. Test first with a small value (`HSTS=300`).

LITH only accepts TLS 1.2 and TLS 1.3: browsers older than about ten years
(Internet Explorer 10 and earlier, Android 4.x) will not be able to connect.

### 8.8 Checklist before publishing a site

- [ ] HTTPS in place: `make TLS=1` + `TLS_PORT`, or a proxy in front of LITH (Caddy, nginx, Cloudflare...).
- [ ] `TLS_REDIRECT=1`, then `HSTS` once HTTPS is validated; private key readable by LITH only.
- [ ] `HIDE_DOTFILES=1` and no secret file in `public/`.
- [ ] Long and unique `AUTH` / `STATUS_AUTH` passwords, `lith.conf` protected.
- [ ] `RATE_LIMIT` configured (in LITH or in the proxy in front).
- [ ] A tested `CSP`.
- [ ] LITH run by an unprivileged user (not `root`).
- [ ] `lith check` without errors.

---

## Chapter 9 — Development mode (live reload)

### 9.1 Using it

```bash
lith start --dev
```

Open your site, edit a file in `public/` and save: the browser reloads **by
itself** in about one second. No more F5!

`--dev` enables `LIVE_RELOAD=1` and `CACHE_CONTROL=no-store` (the browser keeps
nothing in cache, you always see the latest version). You can also put
`LIVE_RELOAD=1` in `lith.conf`.

### 9.2 How it works

1. When LITH sends an HTML page, it adds a small script (≈ 300 characters)
   right before `</body>`.
2. This script opens a **WebSocket** to `/__lith/livereload`: a connection
   that stays open and lets the server send a message to the browser at any time.
3. On its side, LITH scans the `public/` folder every second. As soon as a
   file is added, modified or deleted, it sends the `reload` message to every
   connected browser.
4. The script receives `reload` and reloads the page. If LITH restarts, the
   script reconnects by itself.

The HTML file on your disk is never modified: the script is only added to the
response that is sent.

### 9.3 Limits

- For development only: do not enable it in production.
- Up to 64 tabs connected at the same time.
- Hidden folders and `node_modules/` are not watched.
- In dev mode, HTML pages are neither compressed nor cached (other files are).

---

## Chapter 10 — Reverse proxy and load balancing

### 10.1 The principle

Your site has a static part (HTML, CSS) and an API written in Node.js, Python,
Go, PHP... running on port 3000. Rather than exposing two addresses, LITH
serves the files and **forwards** to the application everything that starts
with `/api`:

```text
                            ┌───────────────────────────┐
  browser ──▶ LITH :8090    │ /api/...  ──▶ app :3000   │
                            │ the rest  ──▶ public/     │
                            └───────────────────────────┘
```

```ini
PROXY=/api http://127.0.0.1:3000
```

The path is forwarded **as is**: `/api/users?id=4` reaches the application as
`/api/users?id=4`. Every method (`GET`, `POST`, `PUT`, `DELETE`...) and the
request body are forwarded.

### 10.2 Complete example with Node.js

`app.js`:

```js
const http = require('http');
http.createServer((req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.end(JSON.stringify({ message: 'Hello from Node', path: req.url }));
}).listen(3000, '127.0.0.1');
```

`lith.conf`:

```ini
PORT=8090
PROXY=/api http://127.0.0.1:3000
```

```bash
node app.js          # in a first terminal
lith start           # in a second one
```

`http://localhost:8090/` shows your site, and `http://localhost:8090/api/test`
answers `{"message":"Hello from Node","path":"/api/test"}`.

### 10.3 Several copies of the application: load balancing

To handle more traffic or keep working if one copy fails, run several copies
of the application and list them:

```ini
PROXY=/api http://127.0.0.1:3000, http://127.0.0.1:3001, http://10.0.0.5:3000
```

- Requests are distributed **in turn** (*round-robin*).
- If a copy does not answer, LITH marks it **out of service**, moves on to the
  next one, and writes it in the log ("backend out of service"):
  `WARN  Proxy : backend 127.0.0.1:3001 hors service`.
- Every `HEALTH_CHECK_INTERVAL` seconds, LITH tests the copies again; as soon
  as a copy answers again, it is brought back.
- If no copy answers: `502 Bad Gateway`. If a copy takes more than
  `PROXY_TIMEOUT` seconds to answer: `504 Gateway Timeout`.

The state of each backend is visible in `/__lith/status`.

### 10.4 What the application receives

LITH adds standard headers so that the application knows the real visitor:

| Header | Content |
|--------|---------|
| `X-Forwarded-For` | Visitor's IP address (appended to the existing list) |
| `X-Real-IP` | Visitor's IP address |
| `X-Forwarded-Host` | The `Host` requested by the visitor |
| `X-Forwarded-Proto` | `https` if the visitor came in on `TLS_PORT`, otherwise `http` |

The original `Host` header is kept. The `X-Forwarded-*` and `X-Real-IP` headers
sent by the visitor themselves are replaced (except `X-Forwarded-For`, which is
appended to): the application can therefore trust `X-Forwarded-Proto` to know
whether the connection was encrypted (`Secure` cookies, absolute links...). The
LITH → backend link stays in plain HTTP: keep your backends on `127.0.0.1` or
on a private network.

### 10.5 Several routes

```ini
PROXY=/api http://127.0.0.1:3000
PROXY=/auth http://127.0.0.1:4000
PROXY=/api/v2 http://127.0.0.1:5000
```

The most specific route wins: `/api/v2/users` goes to port 5000, `/api/users`
to port 3000. A `PROXY=/ http://...` route forwards **everything** (LITH then
becomes a pure proxy in front of your application).

### 10.6 Limits of 1.0.0

- `http://` backends only (no `https://`).
- A new connection to the backend for each request, and the connection with
  the visitor is closed after a proxied response (simple and robust, slightly
  slower than reusing connections).
- WebSockets are not forwarded to backends (`501`).
- Request bodies sent "in chunks" (`Transfer-Encoding: chunked`) are rejected
  (`411`): the client must send `Content-Length` (forms and most HTTP clients do).

---

## Chapter 11 — Monitoring and logs

### 11.1 The terminal log

Two kinds of lines:

```text
[2026-10-18 10:00:00] INFO  LITH 1.0.0 prêt : http://localhost:8090 ...
[2026-10-18 10:00:05] 127.0.0.1 "GET /index.html HTTP/1.1" 200 772 0ms "Mozilla/5.0 ..."
```

- Server **messages**, with their level (`DEBUG`, `INFO`, `WARN`, `ERROR`).
- The **access log**: one line per request:
  `IP "METHOD PATH VERSION" CODE BYTES DURATION "BROWSER"`.

Settings: `LOG_LEVEL=debug` to see everything (useful for troubleshooting),
`LOG_FILE=lith.log` to write to a file, `ACCESS_LOG=0` or `-q` to hide the
access log.

### 11.2 `/__lith/health`: "are you alive?"

Answers `200 OK` with the text `OK`. Open to everyone. Used by Docker,
Kubernetes, load balancers and the `lith health` command.

### 11.3 `/__lith/status`: the JSON dashboard

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

**Who can see it?** By default, only the machine itself (`127.0.0.1`), and
never a request relayed by a proxy. To access it remotely, set
`STATUS_AUTH=user:password`. To disable it completely: `STATUS=0`.

### 11.4 `/__lith/metrics`: for Prometheus and Grafana

The same statistics in the text format of **Prometheus**, the most widely used
monitoring tool (often paired with **Grafana** for charts):

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

Prometheus configuration (`prometheus.yml`):

```yaml
scrape_configs:
  - job_name: lith
    metrics_path: /__lith/metrics
    static_configs:
      - targets: ['127.0.0.1:8090']
    # if STATUS_AUTH is set:
    # basic_auth: { username: admin, password: password }
```

---

## Chapter 12 — Going to production

### 12.1 Choosing your setup

| Situation | Recommended solution |
|-----------|----------------------|
| A Linux server (VPS, Raspberry Pi) | **systemd** (12.3) + built-in HTTPS (`make TLS=1`) or **Caddy** (12.4) |
| A container platform (Docker, Kubernetes, Cloud Run...) | **Docker** (12.5) |
| Quick test on a Linux server | `lith start -d` / `lith stop` (12.2) |
| Windows | Task Scheduler (12.6) |

### 12.2 Daemon mode (Linux/macOS)

```bash
lith start -d
```

LITH detaches from the terminal (you can close it) and keeps running. It
prints where its log and PID file are and how to stop it:

```text
LITH démarre en arrière-plan sur le port 8090.
  Journal : /home/me/my-site/lith.log
  PID     : /home/me/my-site/lith.pid
  Arrêt   : lith stop
```

`lith stop` (run from the same folder, or with `-c` pointing to the same
configuration file) reads the PID file, requests a graceful shutdown and waits
for it to finish.

### 12.3 With systemd (recommended on Linux)

systemd starts LITH with the machine and restarts it if something goes wrong.
Create `/etc/systemd/system/lith.service`:

```ini
[Unit]
Description=LITH - web server
After=network.target

[Service]
WorkingDirectory=/srv/my-site
ExecStart=/usr/local/bin/lith start
Restart=on-failure
User=www-data
Group=www-data
# To listen on port 80 without being root:
AmbientCapabilities=CAP_NET_BIND_SERVICE
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
```

> With systemd, **do not use `-d`**: systemd manages the background itself.

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now lith     # start now and at every boot
systemctl status lith                # status
journalctl -u lith -f                # live log
sudo systemctl restart lith          # after changing lith.conf
```

### 12.4 Adding HTTPS

Common prerequisites: a domain name pointing to your server, and ports 80 and
443 open. Two methods; the first keeps a single program, the second fully
automates certificates.

#### Method A: HTTPS built into LITH (`make TLS=1`) + Let's Encrypt

1. Build and install LITH with HTTPS (chapter 2.3):

   ```bash
   make TLS=1 && sudo install -m 755 bin/lith /usr/local/bin/lith
   ```

2. First start LITH **in plain HTTP only** on port 80 (systemd service from
   12.3, with `PORT=80`): a reachable site is needed to obtain the first
   certificate.

3. Get the certificate with [certbot](https://certbot.eff.org) in *webroot*
   mode: certbot drops a file into `public/.well-known/acme-challenge/`, which
   LITH serves (this folder is never hidden, nor redirected to HTTPS).

   ```bash
   sudo certbot certonly --webroot -w /srv/my-site/public -d mysite.com
   ```

4. The files in `/etc/letsencrypt/live/` are readable by `root` only. Copy
   them for the LITH user, on every renewal, with a small script
   `/etc/letsencrypt/renewal-hooks/deploy/lith.sh` (make it executable with
   `chmod +x`):

   ```bash
   #!/bin/sh
   install -d -o www-data -m 700 /etc/lith
   install -o www-data -m 600 "$RENEWED_LINEAGE/fullchain.pem" "$RENEWED_LINEAGE/privkey.pem" /etc/lith/
   systemctl restart lith      # LITH reads the certificate again at startup
   ```

   Run it once by hand:
   `sudo RENEWED_LINEAGE=/etc/letsencrypt/live/mysite.com sh /etc/letsencrypt/renewal-hooks/deploy/lith.sh`.

5. Enable HTTPS in `lith.conf`, check, restart:

   ```ini
   PORT=80
   TLS_PORT=443
   TLS_CERT=/etc/lith/fullchain.pem
   TLS_KEY=/etc/lith/privkey.pem
   TLS_REDIRECT=1
   HSTS=300            # switch to 31536000 once everything works
   ```

   ```bash
   lith check && sudo systemctl restart lith
   ```

Your site answers at `https://mysite.com`; `http://mysite.com` redirects to it.
certbot renews the certificate automatically and the script above restarts
LITH (a few milliseconds of interruption). The
`AmbientCapabilities=CAP_NET_BIND_SERVICE` line from 12.3 allows ports 80 and
443 without `root`.

#### Method B: Caddy in front of LITH

[Caddy](https://caddyserver.com) automatically obtains and renews free HTTPS
certificates (Let's Encrypt), without any script. LITH can then stay built
without TLS.

1. Configure LITH to listen locally only:

   ```ini
   HOST=127.0.0.1
   PORT=8090
   ```

2. Install Caddy, then write `/etc/caddy/Caddyfile`:

   ```text
   mysite.com {
       reverse_proxy 127.0.0.1:8090
   }
   ```

3. `sudo systemctl reload caddy`. Your site is available at
   `https://mysite.com`, with the padlock.

With **nginx**, the equivalent is a `location / { proxy_pass http://127.0.0.1:8090; }`
block in a server configured with a certificate (for example through `certbot`).

> **Tip:** Behind Caddy or nginx, every request reaches LITH from `127.0.0.1`:
> configure rate limiting in the proxy in front, and use `STATUS_AUTH` if you
> want to view `/__lith/status` remotely (LITH automatically refuses relayed
> requests without a password).

### 12.5 Deploying with Docker

The provided `Dockerfile` builds LITH as a static executable and produces a
**`scratch`** (empty) image that only contains LITH and the site: about
**330 KB**, with no shell or operating system to update.

```bash
docker build -t lith .

# Serve the sample site
docker run -d -p 8090:8090 --name lith lith

# Serve your own site (mounted read-only)
docker run -d -p 8090:8090 -v "$PWD/public:/srv/public:ro" lith

# With your own configuration
docker run -d -p 8090:8090 -v "$PWD/lith.conf:/lith.conf:ro" -v "$PWD/public:/srv/public:ro" lith
```

Inside the container: the configuration is `/lith.conf`, the site
`/srv/public` (see `docker/lith.conf`). LITH runs there without privileges
(user 65534), stops gracefully with `docker stop`, and a **HEALTHCHECK** probe
(`lith health`) shows its state in `docker ps`.

The image is built **without HTTPS** to stay tiny: on a container platform,
encryption is usually handled by the *ingress* or the load balancer in front
of the container.

**docker-compose.yml**:

```yaml
services:
  web:
    build: .
    ports: ["8090:8090"]
    volumes:
      - ./public:/srv/public:ro
    restart: unless-stopped
```

**Kubernetes**: use `/__lith/health` for probes:

```yaml
livenessProbe:
  httpGet: { path: /__lith/health, port: 8090 }
readinessProbe:
  httpGet: { path: /__lith/health, port: 8090 }
```

### 12.6 On Windows

Daemon mode does not exist on Windows. To start LITH with the machine:

1. Open the **Task Scheduler** → *Create Task*.
2. *General*: check "Run whether user is logged on or not".
3. *Triggers*: "At startup".
4. *Actions*: program `C:\lith\bin\lith.exe`, arguments `start`,
   "Start in": your site's folder (the one containing `lith.conf`).

To stop it: `taskkill /IM lith.exe`, or from the Task Scheduler.

### 12.7 Updating LITH

1. Download (or build) the new version.
2. Read the [CHANGELOG](CHANGELOG.md).
3. Run `lith check` with your current configuration.
4. Replace the executable and restart (`systemctl restart lith`,
   `lith stop && lith start -d`, or a new Docker image).

---

## Chapter 13 — How LITH works inside

This chapter is for the curious and for contributors. It is not needed to use LITH.

### 13.1 Overview

```text
                    ┌────────────────────────── lith process ─────────────────────────┐
                    │                                                                 │
  TCP connections ─▶│ main thread: accept() on PORT (and TLS_PORT if HTTPS)           │
                    │        │                                                        │
                    │        ▼                                                        │
                    │  FIFO queue (bounded by MAX_PENDING)                            │
                    │        │                                                        │
                    │        ▼                                                        │
                    │  pool of N worker threads ──▶ read request ──▶ router           │
                    │                                                    │            │
                    │        ┌────────────┬──────────────┬───────────────┼─────────┐  │
                    │        ▼            ▼              ▼               ▼         ▼  │
                    │   /__lith/*     AUTH (401)     PROXY          static      errors│
                    │   status,       Basic          backends       files             │
                    │   metrics                      round-robin    cache, gzip       │
                    │                                                                 │
                    │ side threads: health checks · live reload watcher ·             │
                    │               one light thread per WebSocket client             │
                    └─────────────────────────────────────────────────────────────────┘
```

### 13.2 The journey of a request

1. **Accept**: the main thread waits for connections (`select()` with a 500 ms
   timeout so it can stop cleanly), gets the client's IP address and puts the
   connection in the queue.
2. **Pick-up**: a free worker thread takes it from the queue. If it came in on
   `TLS_PORT`, a TLS session is created; the handshake takes place while the
   first request is read, under the same timeouts.
3. **Reading headers**: the thread reads until it finds the empty line
   (`\r\n\r\n`), in a buffer of at most 16 KB, while watching the timeouts.
4. **Parsing** (`http_parser.c`): request line, headers, path decoding,
   consistency checks. Any anomaly gives a precise error (400, 414, 431, 505...).
5. **Body**: if `Content-Length` is present (and ≤ `MAX_BODY_SIZE`), the body
   is read entirely into memory.
6. **Routing** (`router.c`), in this order: rate limit → known method → safe
   path → internal `/__lith/` routes → `AUTH` → `PROXY` → `OPTIONS` →
   `GET`/`HEAD` on files.
7. **Response** (`response.c`): building the common headers (date, server,
   security, CORS, keep-alive), then sending the body.
8. **Log**: one access line, counters updated.
9. **Next**: if the connection stays open (keep-alive), the thread handles the
   next request (possibly already received: *pipelining*), otherwise it closes
   it and goes back to the queue.

### 13.3 Modules

| Module | Responsibility |
|--------|----------------|
| `platform.c` | The only file that knows the Windows/POSIX differences: sockets, UTF-8 files, time, memory |
| `server.c` | Listening (HTTP and HTTPS), thread pool, connection loop, graceful shutdown |
| `tls.c` | Connection I/O (`LithStream`): direct socket, or through the TLS module when the connection is encrypted |
| `tls/` (Rust) | Optional HTTPS module (`make TLS=1`): rustls behind a small C API (`tls/lith_tls.h`) |
| `http_parser.c` | HTTP parsing, MIME types, HTTP dates, `Range` ranges, path safety |
| `router.c` | Routing and internal routes |
| `static.c` | Files: 304, Range, cache, gzip, chunked sending / `sendfile`, directory listing, SPA |
| `response.c` | Response headers and error pages |
| `cache.c` | Hash table + LRU + reference counting |
| `crypto.c` | SHA-1, Base64, CRC-32, deflate/gzip compressor |
| `security.c` | Basic authentication, per-IP token bucket |
| `proxy.c` | Reverse proxy, round-robin, health checks |
| `websocket.c` | WebSocket protocol and live reload |
| `metrics.c` | Atomic counters and JSON / Prometheus documents |
| `config.c`, `logger.c`, `daemon.c`, `strbuf.c`, `main.c` | Configuration, logs, daemon, strings, command line |

### 13.4 Some design choices

- **A thread pool rather than an event loop** (`epoll`/`kqueue`): the code
  stays simple, readable and identical on the three systems. The number of
  threads is high by default (idle threads cost almost nothing) and LITH
  releases idle connections when clients are waiting.
- **A TLS module that never touches the network**: the Rust code receives the
  bytes read by LITH (`rtls_feed`), returns the plain text (`rtls_read`),
  encrypts responses (`rtls_write`) and hands over the bytes to send
  (`rtls_take_output`). Sockets, timeouts, keep-alive and anti-slowloris
  protections stay in the C code, identical for HTTP and HTTPS. Over HTTPS,
  `sendfile` (zero copy) is replaced by chunked sending, since the data has to
  be encrypted.
- **A reference-counted cache**: a thread sending a file to a slow client
  never blocks the others, and a file can be replaced or evicted while it is
  being sent, safely.
- **Zero dependencies**: SHA-1, Base64, CRC-32 and gzip compression are
  implemented inside LITH (≈ 400 lines). The compressor uses LZ77 with fixed
  Huffman codes: slightly less efficient than zlib at its maximum level, but
  very fast, easy to verify, and the result is cached.
- **UTF-8 paths everywhere**: on Windows, LITH converts paths to the Unicode
  APIs (`_wfopen`, `FindFirstFileW`) to handle accented characters.
- **Graceful shutdown**: signals (Ctrl+C, `SIGTERM`) only raise an atomic
  flag; threads see it and finish their current work.
- **No mutable shared state** outside protected structures (cache, rate
  limiting) and atomic counters: the configuration is read-only once the
  server has started.

### 13.5 Known limits of 1.0.0

| Limit | Value |
|-------|-------|
| Size of a request's headers | 16 KB, 64 headers |
| Length of a URL | 2048 characters |
| WebSocket clients (live reload) | 64 |
| `PROXY` routes / backends per route | 16 / 8 |
| `AUTH` rules | 16 |
| On-the-fly compression (files outside the cache) | files ≤ 8 MB |
| Directory listing (`AUTO_INDEX`) | 10,000 entries |

---

## Chapter 14 — Building, testing and contributing

### 14.1 `make` commands

| Command | Effect |
|---------|--------|
| `make` | Builds `bin/lith` |
| `make test` | Builds and runs the unit tests |
| `make integration` | Runs the integration tests (bash + curl) |
| `make DEBUG=1` | Debug build, with AddressSanitizer and UBSan (except on Windows) |
| `make STATIC=1` | Static executable (no shared library required) |
| `make TLS=1` | Adds HTTPS (Rust `tls/` module, requires `cargo`). Repeat it on every command: `make TLS=1 test`, `make TLS=1 integration` |
| `make CFLAGS="-O3 -march=native"` | Custom compiler options |
| `make install` / `make uninstall` | Installs / removes `lith` in `$(PREFIX)/bin` (default `/usr/local`) |
| `make clean` | Removes `build/` and `bin/` |
| `make distclean` | Also removes the Rust build (`tls/target/`) |

### 14.2 Tests

- **Unit tests** (`tests/unit_tests.c`): HTTP parsing (valid and malicious
  requests), path safety, MIME, dates, ranges, SHA-1, Base64, CRC-32,
  WebSocket, configuration... The gzip compressor is checked by an independent
  mini-decompressor written in the test.
- **Integration tests** (`tests/integration.sh`): real LITH servers are
  started, then queried with `curl` and raw TCP connections: files, security,
  304, Range, gzip, limits, keep-alive, pipelining, slowloris, CORS, rate
  limiting, proxy with backend failure, WebSocket and reload frame, protected
  statistics, log file. On Linux/macOS, they add `SIGTERM` shutdown, daemon
  mode, no file descriptor leaks and an ApacheBench load test.
- **HTTPS tests** (in `tests/integration.sh`): with a `make TLS=1` executable,
  a disposable self-signed certificate is generated with `openssl` in a
  temporary folder (never in the repository), then they check: certificate,
  ALPN, TLS 1.2/1.3, HSTS, `TLS_REDIRECT` redirects, files, Range, gzip, POST,
  keep-alive, proxy, `wss://` WebSocket, rejection of plain HTTP and cutting off
  handshakes that are too slow. With the standard executable, the script checks
  that `TLS_PORT` is rejected with a clear message.

Continuous integration (GitHub Actions) runs all of this on Linux (including a
run under sanitizers, with and without HTTPS), macOS and Windows (with and
without HTTPS), and builds the Docker image, on every `push`.

### 14.3 Adding a feature

1. Read [CONTRIBUTING.md](CONTRIBUTING.md) (code and license rules).
2. Create a branch: `git checkout -b feature/my-feature`.
3. System-specific code → `platform.c`. New setting → `config.h`,
   `config_apply()`, `config_print()`, the `LITH_DEFAULT_CONF` template and
   chapter 5 of this document.
4. Add tests, check `make test`, `make integration` and
   `make DEBUG=1 && make test DEBUG=1`.
5. Open a *Pull Request*.

---

## Chapter 15 — Troubleshooting and FAQ

**The port is already in use.**
Another program is already listening on that port. Change the port
(`lith start 8091`) or find the culprit: `netstat -ano | findstr :8090`
(Windows), `sudo lsof -i :8090` (Linux/macOS).

**"Permission denied" on port 80 (Linux).**
Ports below 1024 are reserved for the administrator. Use a port ≥ 1024 behind
Caddy/nginx, or `AmbientCapabilities=CAP_NET_BIND_SERVICE` with systemd (12.3).
Do not run LITH as `root`.

**My page shows up, but not the CSS or the images.**
Check the paths in your HTML: `href="/css/style.css"` (from the root) or
`href="css/style.css"` (relative to the page). Open the browser console
(F12 → *Network*) to see which addresses answer `404`. Watch out for
uppercase: on Linux, `Logo.png` and `logo.png` are two different files.

**I edit a file but the browser shows the old version.**
The browser uses its cache. Press Ctrl+F5, or use `lith start --dev`. Also
check `CACHE_CONTROL`: with a high `max-age`, the browser does not ask for the
file again.

**My React application routes give 404 when I reload.**
Enable `SPA_FALLBACK=1` (6.4).

**My phone cannot display the site.**
Same Wi-Fi? `HOST=0.0.0.0`? Firewall allowed? Use the computer's IP address,
not `localhost` (6.9).

**`/__lith/status` answers 403.**
That is intended: without `STATUS_AUTH`, it is only visible from the machine
itself, with no proxy in front. Set `STATUS_AUTH=user:password`.

**The proxy answers 502.**
The application behind it does not answer: is it running? On the right address
and port? The LITH log tells which backend is "hors service" (out of service).
Test it directly: `curl http://127.0.0.1:3000/`.

**`lith stop` cannot find the PID file.**
Run it from the same folder as `lith start -d` (or with the same `-c`), because
the `PID_FILE` path is relative to that folder.

**How do I see what is happening in detail?**
`LOG_LEVEL=debug`, then watch the log. `lith check` shows the configuration
actually in use.

**Is LITH solid enough for production?**
Yes, for serving sites and applications of reasonable size: it is tested
automatically (including under memory sanitizers), protects against common
attacks and shuts down cleanly. For a public site, enable HTTPS (`make TLS=1`,
or a proxy in front: 12.4).

**`lith check` says "compilé sans HTTPS" (built without HTTPS).**
Your executable was built without the HTTPS option. Rebuild with `make TLS=1`
(chapter 2.3), or remove `TLS_PORT` from `lith.conf`.

**HTTPS: "certificate unreadable" or "the certificate and the key do not match".**
`TLS_CERT` must contain the certificate in PEM format (text starting with
`-----BEGIN CERTIFICATE-----`), preferably the full chain (`fullchain.pem`);
`TLS_KEY` the matching private key (`privkey.pem`). A `.pfx`/`.p12` file
(Windows) can be converted with
`openssl pkcs12 -in site.pfx -nokeys -out cert.pem` and
`openssl pkcs12 -in site.pfx -nocerts -nodes -out key.pem`.

**HTTPS: the browser shows "Your connection is not private".**
Expected with a self-signed certificate (local tests): the browser does not
know it. In production, use a certificate from a recognized authority (Let's
Encrypt, free: 12.4). If the warning mentions the name, the requested domain is
not in the certificate. If the site does not load at all, make sure you use
`https://` with `TLS_PORT` (an `http://` request on the HTTPS port is rejected).

**Can I run PHP?**
Not directly. Run PHP with its built-in server or PHP-FPM behind a compatible
server, then use `PROXY` (chapter 10).

---

## Chapter 16 — Roadmap and technical choices (including Rust)

### 16.1 Where 1.0.0 comes from

1.0.0 takes the realistic ideas from the project's original roadmap and
delivers them in a first stable, tested release. The old intermediate version
numbers were dropped: LITH starts again cleanly at **1.0.0** and will now
follow semantic versioning (`MAJOR.MINOR.PATCH`).

**Already delivered in 1.0.0**:

| Original idea | Status in 1.0.0 |
|---------------|-----------------|
| Rate limiting / anti-DDoS | `RATE_LIMIT`, per-IP token bucket, `429` |
| Security headers | `SECURITY_HEADERS`, `CSP`, `CORS_ORIGIN` |
| Gzip compression | built in, no zlib, cached |
| WebSockets | RFC 6455, used by automatic reload |
| Reverse proxy | `PROXY` |
| Load balancing | round-robin + failover |
| Server health checking | periodic health checks |
| Basic authentication | `AUTH` |
| Monitoring dashboard | `/__lith/status` (JSON), protected |
| Prometheus metrics | `/__lith/metrics` |
| Zero-copy (`sendfile`) | on Linux |
| Containerization | `scratch` image ≈ 330 KB + health check |

**Delivered since**: **native HTTPS** (`make TLS=1` option, Rust module +
rustls, see 16.4), with HTTP → HTTPS redirect and HSTS.

### 16.2 Recommended next steps

In order of priority (value for users / effort):

1. ~~**Native HTTPS (TLS)**~~ — delivered (`make TLS=1`). Possible follow-ups:
   automatic certificate retrieval (built-in ACME / Let's Encrypt), several
   certificates depending on the domain name (SNI), `https://` backends for the
   proxy, HTTPS in the Docker image and in the published executables.
2. **Hot configuration reload** (`SIGHUP`) without dropping connections,
   including reloading the certificate after renewal.
3. **Configurable redirects and rewrites** (`REDIRECT=/old /new 301`), then
   pattern-based routing.
4. **Proxy improvements**: reusing connections to backends, forwarding
   WebSockets, "chunked" bodies.
5. **CGI / FastCGI** to run PHP or Python directly.
6. **Event loop** (`epoll`, `kqueue`, IOCP) as an option, for very large
   numbers of concurrent connections.
7. **Stronger compression** (dynamic Huffman) and pre-compressed **Brotli**
   (`file.css.br` served if it exists).
8. **JWT tokens** to protect APIs.
9. **HTTP/2**, then possibly **HTTP/3**, preferably through a proven library
   rather than a home-made rewrite.
10. **Plugins** (dynamically loaded libraries) once the internal API is stable.

### 16.3 What was deliberately left out

The original roadmap also contained ideas that do not belong to a web server,
or not to current technology: quantum computing, DNA storage, interplanetary
networking, AI self-rewriting code, a dedicated operating system, FPGA, etc.
They were removed: a project that promises everything delivers nothing. LITH's
strength is to be **small, safe, portable and understandable**; every new
feature must strengthen these qualities. A "mini-WAF" based on SQL/XSS patterns
was also left out for now: such filters easily block legitimate users and give
a false sense of security; a CSP and correct application code protect better.

### 16.4 Should LITH be rewritten in Rust?

**Recommendation: no rewrite. Keep the core in C11, and bring in Rust only
where it adds the most value: an optional HTTPS module.**

Why **not** rewrite everything:

- The 1.0.0 C code is now modular, tested (unit, integration,
  AddressSanitizer, UBSan) and dependency-free. A rewrite would start from
  scratch and delay the real new features accordingly.
- LITH's identity is lightness and portability: a 300 KB executable that
  builds with any C compiler, down to small embedded devices (routers,
  connected objects). A Rust equivalent is larger and requires a heavier
  toolchain, less available on those targets.
- The classic risks of C (buffer overflows, leaks) are contained here by a
  simple architecture, bounded functions, strict input validation and tests
  under sanitizers.

Where Rust **really** makes sense: **HTTPS**. Encryption and certificate
parsing are the most delicate and most exposed parts of a server; that is where
a memory bug costs the most. The Rust library **rustls** is modern,
memory-safe and has been audited.

This is what was implemented:

```text
   LITH (C11)  ──── minimal C API (FFI) ────▶  lith_tls (Rust + rustls)
   - server, HTTP, files, proxy                - TLS handshake
   - sockets, timeouts, keep-alive             - encryption / decryption
   - tls.c: LithStream (plain or TLS)          - certificate loading
```

- The `tls/` folder contains a Rust library built as a static library
  (`crate-type = ["staticlib"]`, under 300 lines), which exposes about a dozen
  C functions declared in `tls/lith_tls.h`: create a context with a
  certificate and a key (`rtls_config_new`), accept a connection
  (`rtls_accept`), read (`rtls_feed` + `rtls_read`), write (`rtls_write` +
  `rtls_take_output`), close (`rtls_close`).
- The API is "buffer-based": Rust never sees the socket. The C code stays in
  charge of the network, and therefore of timeouts and protections; no
  difference in behavior between HTTP and HTTPS apart from encryption.
- The crypto provider is **ring** (no need for cmake or nasm), Rust panics are
  set to `abort` and no function panics across the C boundary. Dependency
  versions are pinned by `tls/Cargo.lock` and the build uses `--locked`.
- Enabled only with `make TLS=1`: without this option, LITH stays 100% C, with
  no dependencies, exactly as before; `tls.c` then boils down to direct
  `recv`/`send` calls.

Alternative ruled out: **mbedTLS** (C, lightweight, designed for embedded
systems). Simpler to integrate without a Rust toolchain, but it would have
added C code to the most exposed part of the server; rustls offers a better
memory-safety guarantee. The separation through `tls.c` would nevertheless
allow adding a second module (mbedTLS) for targets where Rust is not available.

In short: **C for the core (light, portable, already solid), Rust as an
optional module for HTTPS (where memory safety matters most)**. A complete
rewrite would only be worth reconsidering if LITH became a much larger project
(HTTP/2, HTTP/3, plugins...) carried by a team comfortable with Rust.

---

## Chapter 17 — Glossary

| Term | Definition |
|------|------------|
| **Backend** | Application placed behind LITH, to which it forwards requests (proxy) |
| **Cache** | A copy kept at hand to go faster (in RAM on the server side, on disk on the browser side) |
| **Client** | The program making the request: browser, `curl`, mobile app... |
| **CORS** | Rules that allow (or not) a site to read the responses of another one |
| **CSP** | *Content-Security-Policy*: list of allowed sources for scripts, styles, images |
| **Daemon** | Program running in the background, without a terminal |
| **ETag** | Tag identifying a version of a file, for `304` responses |
| **gzip** | Compression format used on the web |
| **Header** | `Name: value` information line of an HTTP request or response |
| **Health check** | Automatic check that a server answers |
| **HTTP / HTTPS** | The web protocol / its encrypted version |
| **IP (address)** | Number identifying a machine on a network (`192.168.1.20`) |
| **Keep-alive** | Reusing one connection for several requests |
| **localhost** | Your own machine (`127.0.0.1`) |
| **LRU** | *Least Recently Used*: what was used least recently is evicted first |
| **MIME (type)** | Nature of a content (`text/html`, `image/png`...) |
| **PATH** | List of folders where the system looks for commands |
| **PID** | Number of a running process |
| **Pipelining** | Sending several requests in a row without waiting for the responses |
| **Port** | "Door" number of a network program on a machine |
| **Prometheus** | Monitoring tool that collects metrics |
| **Request / response** | The client's request / what the server sends back |
| **Reverse proxy** | Server that receives requests and forwards them to another server |
| **Round-robin** | Distribution in turn |
| **Slowloris** | Attack that ties up a server by sending requests extremely slowly |
| **SPA** | *Single Page Application*: web application on a single page (React, Vue...) |
| **Thread** | Thread of execution: lets a program do several things at the same time |
| **TLS** | The encryption technology behind HTTPS |
| **URL** | Full address of a web resource |
| **WebSocket** | Permanent, two-way connection between browser and server |

---

## Appendix A — Cheat sheet

```bash
lith init my-site             # create a project
lith start                    # start (port from lith.conf, 8090 by default)
lith start 8080 --dev         # another port + auto reload
lith start -r dist -p 3000    # another folder, another port
lith start -d && lith stop    # background (Linux/macOS)
lith check                    # validate the configuration
lith health                   # is the server answering?
```

```ini
# lith.conf - the most useful settings
PORT=8090
PUBLIC_DIR=public
SPA_FALLBACK=1
RATE_LIMIT=50
CSP=default-src 'self'
AUTH=/admin admin:password
PROXY=/api http://127.0.0.1:3000
STATUS_AUTH=admin:password
LOG_FILE=lith.log
# HTTPS (executable built with: make TLS=1)
TLS_PORT=443
TLS_CERT=fullchain.pem
TLS_KEY=privkey.pem
TLS_REDIRECT=1
HSTS=31536000
```

| Address | Role |
|---------|------|
| `/__lith/health` | Availability |
| `/__lith/status` | JSON statistics |
| `/__lith/metrics` | Prometheus |

---

## Appendix B — HTTP status codes used by LITH

| Code | Name | When LITH sends it |
|------|------|--------------------|
| 101 | Switching Protocols | Opening a WebSocket (live reload) |
| 200 | OK | File sent |
| 204 | No Content | Response to `OPTIONS` |
| 206 | Partial Content | Piece of a file (`Range`) |
| 301 | Moved Permanently | `/folder` → `/folder/`; `http://` → `https://` (`TLS_REDIRECT`, `GET`/`HEAD`) |
| 304 | Not Modified | The browser already has the right version |
| 308 | Permanent Redirect | `http://` → `https://` for `POST`, `PUT`... (`TLS_REDIRECT`) |
| 400 | Bad Request | Malformed request, missing `Host`, invalid path |
| 401 | Unauthorized | Password required (`AUTH`, `STATUS_AUTH`) |
| 403 | Forbidden | Attempt to leave the public folder; status requested remotely |
| 404 | Not Found | Missing or hidden file |
| 405 | Method Not Allowed | `POST`/`PUT`/`DELETE` on a static file |
| 408 | Request Timeout | Request too slow (`REQUEST_TIMEOUT`) |
| 411 | Length Required | "Chunked" body without `Content-Length` |
| 413 | Content Too Large | Body larger than `MAX_BODY_SIZE` |
| 414 | URI Too Long | URL longer than 2048 characters |
| 416 | Range Not Satisfiable | Requested piece outside the file |
| 429 | Too Many Requests | `RATE_LIMIT` exceeded |
| 431 | Request Header Fields Too Large | Headers larger than 16 KB |
| 500 | Internal Server Error | Unexpected problem (rare) |
| 501 | Not Implemented | Unknown method, unsupported encoding, WebSocket to a backend |
| 502 | Bad Gateway | No proxy backend answers correctly |
| 503 | Service Unavailable | Queue full (`MAX_PENDING`) |
| 504 | Gateway Timeout | Backend too slow (`PROXY_TIMEOUT`) |
| 505 | HTTP Version Not Supported | Version other than HTTP/1.x |

---

*LITH 1.0.0 — FomaDev. Distributed under the [Fordima Public License](LICENSE).*
