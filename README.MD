# LITH - Light HTTP Server

LITH is a minimalist, high-performance, multi-threaded HTTP engine implemented in standard C11. Engineered to be exceptionally lightweight, modular, and portable, it provides native support for Windows (via w64devkit/MinGW-w64 and Winsock2), macOS, and Linux systems. It is designed with robust network lifecycle management, resource-efficient concurrency handlers, and zero external software dependencies.

## Key Features

- **Architectural Thread Pool (v1.0.7)**: Instantiates a fixed set of permanent, isolated worker threads upon boot. Eliminates runtime thread spawning/destruction overhead by utilizing a synchronized FIFO task queue managed via POSIX condition variables (`pthread_cond_t`) and mutexes (`pthread_mutex_t`).
- **High-Throughput Main Loop**: Features an optimized accept-loop that offloads incoming sockets instantly to the shared worker pool, freeing the core network thread to handle maximum incoming connection density per second.
- **HTTP Keep-Alive Persistent Connections**: Keeps client sockets active for consecutive resource transfers within a secure window. Features automated connection recycling to maximize performance and minimize TCP handshake overhead.
- **Anti-Slowloris Security Guard**: Arms incoming sockets with a rigid 3-second network receive timeout (`SO_RCVTIMEO`) directly on the accept layer, ensuring resilient defense against slow-connection or resource-exhaustion vector attacks.
- **Advanced I/O POST Payload Streaming**: Features automated network stream processing to fully parse incoming HTTP `POST` requests, handling chunked or fragmented bodies using precise `Content-Length` tracking and multi-stage buffer safety boundaries.
- **Thread-Safe Isolated Configuration**: Passes critical runtime data (such as the web root asset path) directly inside the thread pool context, completely removing reliance on shared static global variables or race-prone states.
- **External Configuration Engine**: Dynamically loads runtime environments from a `lith.conf` configuration file without requiring binary recompilation.
- **Dynamic MIME Type Resolution**: Automatically maps file extensions to their corresponding IANA media types (`.html`, `.css`, `.js`, `.json`, `.png`, etc.) for proper browser rendering.
- **Unified Security & Path Filters**: Protects against directory traversal vulnerabilities by parsing, validating, and rejecting malicious relative path patterns such as `..`.
- **Unified Error Canvas Layouts**: Serves clean, dark-themed HTML error pages for common HTTP status codes (`400`, `403`, `404`, `408`, and `500`).

---

## Quick Start

### 1. Build Compilation

LITH ships with a highly robust, cross-platform `Makefile` optimized for GNU Make. It automatically handles header dependency generation (`-MMD -MP`) and natively adapts to both Linux/macOS shells and Windows environments.

To compile the project from scratch:

```bash
make clean
make
```

### 2. Configuration (`lith.conf`)

LITH loads its runtime configuration from a file named `lith.conf` located in the application's root directory.

```ini
# Network listening port
PORT=8090

# Directory containing static web assets
PUBLIC_DIR=public
```

### 3. Deployment & CLI Usage

Place your web assets inside the configured `public/` directory(e.g., `public/index.html`)..

LITH features a modern, explicit CLI command architecture. To execute the engine, you must use the start command routing path:

```bash
# Launch the engine using defaults or configuration settings
bin/lith start

# Override the configured port dynamically via runtime arguments
bin/lith start 8080
```

To display context help or print the build version string:

```bash
bin/lith --help
bin/lith --version
```

---

## Repository Architecture

```text
.
├── bin/                 # Output directory for compiled executable binaries
├── include/             # Header files, macros, and cross-platform abstractions
│   ├── common.h         # Centralized data structures, configuration state, and global version definitions
│   ├── http_parser.h    # HTTP Request structures, MIME mapping, and error pages
│   ├── http_router.h    # Application routing and static file rendering logic
│   ├── logger.h         # Centralized thread-safe logging interface
│   ├── server.h         # Thread pool definitions, worker prototypes, and socket contexts
│   └── server_utils.h   # Multiplatform OS socket wrappers (lith_close_socket)
├── public/              # Static web root (HTML, CSS, JS, Assets)
├── src/
│   ├── server/          # Configuration parser and server utility modules
│   │   ├── config.c     # Runtime initialization logic (lith.conf reader)
│   │   └── utils.c      # Multiplatform OS abstractions and file system safety
│   ├── http_parser.c    # Case-insensitive protocol parser
│   ├── http_router.c    # Request handler, route dispatcher, and security validation
│   ├── logger.c         # Formatted terminal output subsystem
│   ├── main.c           # Application bootstrap, signal trapping, and CLI routing argument handler
│   ├── server.c         # TCP socket initialization, reuse options, and accept-push loop
│   └── server_worker.c  # Persistent pool worker thread loop, queue management, and request routing
├── lith.conf            # Runtime configuration file
└── Makefile             # Universal, silent multi-platform automated build script
```

---

## Technical Specifications

<table>
    <thead>
        <tr>
            <th>Parameter</th>
            <th>Default Value</th>
            <th>Description</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td><code>LITH_VERSION</code></td>
            <td><code>1.0.6</code></td>
            <td>Current stable release version</td>
        </tr>
        <tr>
            <td><code>BUFFER_SIZE</code></td>
            <td><code>4096 bytes</code></td>
            <td>Standard socket read/write block buffer size</td>
        </tr>
        <tr>
            <td>Maximum Buffer</td>
            <td><code>16 KB</code></td>
            <td>Maximum cumulative buffer space for HTTP headers and POST bodies</td>
        </tr>
        <tr>
            <td><code>BACKLOG</code></td>
            <td><code>10</code></td>
            <td>Maximum number of pending client connections in the listen queue</td>
        </tr>
        <tr>
            <td>Keep-Alive Timeout</td>
            <td><code>3 seconds</code></td>
            <td><code>SO_RCVTIMEO</code> window before cleanly recycling a persistent socket</td>
        </tr>
        <tr>
            <td><code>MAX_KEEP_ALIVE_REQUESTS</code></td>
            <td><code>100</code></td>
            <td>Maximum requests processed per single client TCP session</td>
        </tr>
    </tbody>
</table>

## License

This software is distributed under the terms of the [FomaDev Public License (FPL)](LICENSE). Under this license, personal and educational use of compiled binaries is free, including within corporate environments. Commercial republication, direct source-code integration into competitive proprietary tools, or maintaining standalone independent forks is strictly restricted without explicit commercial authorization. For complete licensing terms, consult the accompanying [LICENSE](LICENSE) file.