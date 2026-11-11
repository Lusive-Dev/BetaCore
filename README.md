# Nexium

Nexium is an original programming-language and developer-tools project.

## Current stage: runtime foundation

This repository currently contains the first runtime foundation. It is not Nexium 1.0 and does not yet include the compiler, IDE, package manager, debugger, shell, or installer.

### Included
- Original C++17 bootstrap runtime API.
- Explicit aligned allocation and deallocation with live-byte and allocation-count tracking.
- Owning byte-string type.
- Generic dynamically-sized array.
- Runtime initialization state, console output, and panic reporting.
- Initial ABI version constants (0.1).
- CMake build and unit tests.
- Windows x64 GitHub Actions build and artifact upload.

### Build
Requires CMake and a C++17 compiler.

```sh
cmake -S . -B build -DNEXIUM_RUNTIME_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Windows, configure with `cmake -S . -B build -A x64`.

### Remaining runtime work
This is a first runtime foundation, not a finished production runtime. UTF-8 validation, filesystem, networking, cryptography, threading, async, platform services, exception/unwind integration, freestanding targets, and a stable external ABI still need implementation and tests.

The bootstrap implementation uses C++17 and host allocation/console facilities as implementation dependencies. The runtime source itself is original project code; no other language runtime source has been copied.
