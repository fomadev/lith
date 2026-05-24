# LITH - Light HTTP Server

LITH is a minimalist, high-performance, multi-threaded HTTP server implemented in standard C11. Built to be exceptionally lightweight and portable, it provides native support for both Windows (via MinGW-w64 and Winsock2) and Linux systems. It is engineered with security and performance in mind, offering full protection against common web vulnerabilities like directory traversal.

## Features

- **Multi-threaded Architecture**: Utilizes pthreads to handle multiple simultaneous client connections efficiently.
- **Static File Delivery**: Automatically serves HTML, web assets, and documents directly from the dedicated `public/` web root directory.
- **Cross-Platform Compatibility**: Complete compilation compatibility with Windows and POSIX-compliant Linux environments using platform-specific socket abstractions.
- **Directory Traversal Protection**: Includes built-in path canonicalization security checks to intercept and block malicious relative path attempts.
- **Embedded Logger**: Real-time terminal logging mechanism detailing timestamps, severity levels, and client transaction records.
- **Zero Dependencies**: Relies exclusively on standard C library wrappers, maintaining a minimal binary footprint.

## Run

### Prerequisites

To build LITH from source, you need:
- A GCC compiler toolchain (MinGW-w64 for Windows setups, or standard GCC for Linux distribution suites).
- GNU Make utility.

### Usage Instructions

1. **Deploy Web Content**: Place your static files (such as `index.html`) inside the `public/` directory at the project root.

2. **Launch the Executable**: Execute the compiled binary from your command-line interface:
    ```bash
    bin\lith
    ```

3. **Specify a Custom Port (Optional)**: By default, the server initializes on port 8080. You can override this behavior by passing a custom port number as a command-line argument:
    ```bash
    bin\lith 8005
    ```

4. **Access the Application**: Open any compatible web browser application and navigate to `http://localhost:8005`.

## Repository Architecture
The codebase follows a modular design pattern to keep concerns separated:

* `src/`: Core logic implementation files containing socket handling, server lifecycle management, HTTP parsing, and logger routines (`.c`).

* `include/`: Global structure declarations, networking macro bindings, definition constants, and platform configuration headers (`.h`).

* `public/`: The localized sandbox serving as the server web root directory.

* `bin/`: The target output directory reserved for compiled binaries and operational executables.

## Server Settings

Default server configurations are managed statically via macros located within `include/server.h` and `include/common.h`:

* `DEFAULT_PORT`: 8080

* `BUFFER_SIZE`: 4096 bytes

* `BACKLOG`: 10 pending connection limits

* `LITH_VERSION`: 1.0.1

## License

This software is distributed under the terms of the [FomaDev Public License (FPL)](LICENSE). Under this license, personal and educational use of compiled binaries is free, including within corporate environments. Commercial republication, direct source-code integration into competitive proprietary tools, or maintaining standalone independent forks is strictly restricted without explicit commercial authorization. For complete licensing terms, consult the accompanying [LICENSE](LICENSE) file.