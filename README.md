# Nexium

Nexium is an original programming-language and developer-tools project.

## Runtime: expanded hosted platform layer (ABI 0.3; not frozen)

The repository now contains a usable host runtime foundation for the compiler to target. This is a completed **bootstrap-runtime milestone**, not a finished Nexium 1.0 language distribution.

### Implemented runtime surface

- Aligned allocation/deallocation with live-byte, peak-byte, and allocation counters.
- Owning byte strings with copy/move operations, UTF-8 validation, and strict Unicode scalar encode/decode.
- Dynamically-sized arrays with overflow checks and safe growth when insertion arguments alias existing elements.
- Console output and fatal panic reporting.
- Binary file open/read/write/flush/close, existence checks, and removal.
- Monotonic nanosecond clock and millisecond sleep.
- Environment-variable retrieval with required-buffer-size reporting.
- Basic threads (start/join/detach), mutexes, and a bounded worker-pool task scheduler with completion/failure states.
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
- The runtime is hosted: it uses C++17, the host C runtime, host threading, and native OS services. It is not a freestanding/kernel runtime.
- TCP currently provides client-side stream operations only. Server/listener sockets, UDP, TLS, certificate validation, and a full networking stack are not implemented.
- Secure random generation is provided, but a complete cryptographic API (hashing, authenticated encryption, key management, and TLS) is not yet implemented. Do not implement cryptographic algorithms in application code as a substitute.
- Process support currently covers launch, wait, terminate, and close; pipes, redirected standard streams, environment/cwd configuration, job objects, and process groups remain future work.
- Allocator replacement hooks, complete Unicode normalization/case-folding/grapheme handling, and a stable long-term ABI remain future work.
- OS-specific permission, metadata, symlink, watcher, and durable atomic-write APIs remain future work.

The implementation in this repository is original Nexium project code. C++17 is used as a bootstrap implementation tool; this does not mean the Nexium compiler or runtime has been implemented by copying another language's runtime.
