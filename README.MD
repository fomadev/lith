# LITH - Light HTTP Server

LITH is a minimalist, high-performance, multi-threaded HTTP server implemented in standard C11. Built to be exceptionally lightweight and portable, it provides native support for Windows (via MinGW-w64 and Winsock2), macOS, and Linux systems. It is engineered with security, robust network lifecycle management, and performance in mind.

## Features

- **Multi-threaded Architecture**: Utilizes pthreads to handle multiple simultaneous client connections efficiently without blocking the core socket listener.
- **Dynamic MIME Type Resolution**: Automatically detects file extensions to serve proper `Content-Type` headers, enabling native web browser rendering for modern multi-resource web applications.
- **Cross-Platform Socket Layer**: Full compilation compatibility across Windows and POSIX-compliant environments using granular compile-time hardware abstractions.
- **Strict Port Collision Guard**: Advanced validation layer preventing concurrent process overlaps on identical network ports, terminating gracefully with precise diagnostic logs.
- **Directory Traversal Protection**: Integrated path canonicalization security filters that immediately intercept and block malicious relative path execution (`..`).
- **Embedded Logger**: Real-time unified console logging mechanism mapping detailed operational timestamps, network states, and transaction levels (`INFO`, `WARN`, `ERROR`).
- **Zero Dependencies**: Relies exclusively on standard operating system C runtime components, maintaining a minimal production binary footprint.

## Run

### Prerequisites

To build LITH from source, you need:
- A GCC compiler toolchain (MinGW-w64 for Windows environments, or native GCC for Linux/macOS suites).
- GNU Make utility.

### Usage Instructions

1. **Deploy Web Content**: Place your web directory trees and static assets (such as `.html`, `.css`, `.js`, `.png`, `.jpg`, `.ico`, `.json`) directly inside the `public/` folder at the root of the project.

2. **Launch the Executable**: Execute the compiled structural binary from your shell command line interface:
    ```bash
    bin/lith
    ```

3. **Specify a Custom Port (Optional)**: By default, the server initializes on port 8090. You can override this behavior dynamically by appending a valid port binding token as an argument:
    ```bash
    bin/lith 8005
    ```

4. **Access the Application**: Open any compatible web browser and target your local loopback infrastructure: `http://localhost:8005`.

## Repository Architecture

The codebase follows a modular design pattern ensuring clear separation of concerns:

* `src/`: Core logical modules handling low-level network sockets, server multi-threaded lifecycle execution, HTTP stream parsing, and internal logging routines (`.c`).
* `include/`: Global structure declarations, networking macro mappings, operational boundary constants, and OS-specific binding configurations (`.h`).
* `public/`: The localized safe sandbox environment acting as the default root for served static payloads.
* `bin/`: The build output directory reserved for finalized executable binaries.

## Server Settings

Default backend configurations are statically optimized via macro bindings located within `include/server.h` and `include/common.h`:

* `DEFAULT_PORT`: 8090
* `BUFFER_SIZE`: 4096 bytes
* `BACKLOG`: 10 maximum pending connection queues
* `LITH_VERSION`: 1.0.2

## Supported Asset Formats

LITH natively decodes and streams the following web layouts:
- **Documents**: `.html`, `.htm`, `.txt`
- **Styles & Scripts**: `.css`, `.js`
- **Data Schemas**: `.json`
- **Graphics**: `.png`, `.jpg`, `.jpeg`, `.gif`, `.ico`
- *Other formats safely fall back to standard `application/octet-stream` byte matching.*

## License

This software is distributed under the terms of the [FomaDev Public License (FPL)](LICENSE). Under this license, personal and educational use of compiled binaries is free, including within corporate environments. Commercial republication, direct source-code integration into competitive proprietary tools, or maintaining standalone independent forks is strictly restricted without explicit commercial authorization. For complete licensing terms, consult the accompanying [LICENSE](LICENSE) file.