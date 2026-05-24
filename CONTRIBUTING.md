# Contributing to LITH

Thank you for your interest in LITH. This project is governed by the FomaDev Public License (FPL). By contributing to this repository, you agree to adhere to the terms described below and accept that your contributions will be bound by the same licensing terms.

## Licensing and Compliance

Before submitting any code, please note the following essential rules regarding the FomaDev Public License (FPL):

1. **Official Integration Only**: Forks are authorized exclusively for the purpose of contributing to the official FomaDev repository. Maintaining separate, standalone, or permanent forks to bypass FomaDev authority is strictly prohibited.
2. **IP Ownership**: By submitting a Pull Request, you agree to grant FomaDev full rights to integrate, modify, and distribute your code under the FPL.
3. **Commercial Restrictions**: Modified versions of this server or its source code cannot be redistributed, resold, or republished on any platform or repository without an explicit commercial license from FomaDev.

## How to Contribute

### Reporting Bugs or Requesting Features

If you encounter a bug (such as security vulnerabilities, crashes, or memory leaks) or have an idea for an improvement, please open an Issue on GitHub rather than writing code immediately. 

When opening an issue, ensure you provide:
* A clear and descriptive title.
* A detailed description of the bug or feature request.
* Steps to reproduce the issue, along with actual and expected behaviors.
* Your environment details (Compiler version, Operating System, Winsock/POSIX environment).

### Submitting a Pull Request (PR)

To submit code modifications, please follow this rigorous process:

1. **Fork the Repository**: Create a temporary fork of the official repository to work on your modifications.

2. **Create a Feature Branch**: Branch out from the main branch using a clear naming convention:
   ```bash
   git checkout -b feature/your-feature-name
   ```
   or
   ```bash
   git checkout -b fix/your-bug-fix
   ```

3. **Write Clean C Code**: Ensure your code complies with the **C11** standard and builds flawlessly on both Windows (MinGW) and Linux systems. Avoid platform-specific functions unless they are properly isolated using compiler directives (`#ifdef _WIN32`). Code must be modular, efficient, and well-commented.

4. **Test Manually**: Always run `make` to compile the server and perform rigorous manual tests (e.g., checking network responses via `curl`, testing directory traversal security, and ensuring proper multi-threaded request execution).

5. **Commit Changes**: Write concise, imperative commit messages describing exactly what has changed:
```bash
git commit -m "Fix memory allocation leak inside client handler context"
```

6. **Push and Open PR**: Push your branch to your temporary fork and open a Pull Request against the official FomaDev main branch.

## Pull Request Review Process

Every Pull Request will undergo manual review by FomaDev. Pull Requests will be evaluated based on:

* **Strict Architectural Integrity**: Code must match the core minimalist design of the LITH networking engine.

* **Code Safety and Security**: High focus on protecting against Directory Traversal, buffer overflows, and memory safety.

* **Performance and Resource Management**: Minimizing memory footprint, avoiding leaks, and ensuring quick socket recycling.

FomaDev reserves the right to request adjustments, rewrite portions of the submitted code, or reject Pull Requests that do not align with the long-term roadmap of the project.

## Support and Inquiries

For inquiries regarding custom commercial licensing, derivative product authorizations, or specific implementation rights, please contact Fordi (FomaDev) directly through the contact details provided in the official documentation.