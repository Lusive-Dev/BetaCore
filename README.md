# Nexium

Nexium is an original programming-language and developer-tools project.

## Runtime milestone: 0.2 bootstrap runtime

The repository now contains a usable host runtime foundation for the compiler to target. This is a completed **bootstrap-runtime milestone**, not a finished Nexium 1.0 language distribution.

### Implemented runtime surface

- Aligned allocation/deallocation with live-byte, peak-byte, and allocation counters.
- Owning byte strings with copy/move operations and UTF-8 validation.
- Dynamically-sized arrays with overflow checks and safe growth when insertion arguments alias existing elements.
- Console output and fatal panic reporting.
- Binary file open/read/write/flush/close, existence checks, and removal.
- Monotonic nanosecond clock and millisecond sleep.
- Environment-variable retrieval with required-buffer-size reporting.
- Basic threads (start/join/detach) and mutexes.
- C-facing bootstrap ABI for versioning, memory, output, panic, file operations, and timing.
- CMake build, automated tests, and Windows x64 CI on Windows Server 2022 and the latest Windows runner.

### Build locally

Requires CMake 3.16+ and a C++17 compiler.

```sh
cmake -S . -B build -DNEXIUM_RUNTIME_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Windows with Visual Studio, configure x64 with:

```powershell
cmake -S . -B build -A x64 -DNEXIUM_RUNTIME_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Runtime contracts and limitations

- The ABI is currently version 0.2 and is a bootstrap contract; it is not yet frozen for long-term binary compatibility.
- Strings store bytes. UTF-8 validity can be checked, but Unicode normalization and higher-level text processing are not provided.
- File paths use the host C runtime's narrow-character path handling. Windows wide-path support and richer filesystem/path APIs remain future work.
- Thread callbacks must not rely on exceptions crossing the runtime boundary; uncaught callback exceptions are contained by the thread trampoline.
- Mutexes must be destroyed only after they are unlocked and no thread is using them.
- The runtime is hosted: it uses C++17, the host C runtime, and the host threading implementation. Freestanding/kernel support, allocator replacement hooks, async/task scheduling, networking, cryptography, process APIs, structured error propagation, and platform-specific services remain future work.

The implementation in this repository is original Nexium project code. C++17 is used as a bootstrap implementation tool; this does not mean the Nexium compiler or runtime has been implemented by copying another language's runtime.
