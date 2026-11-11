# Nexium

Nexium is an original programming-language and developer-tools project.

## Runtime: expanded hosted platform layer (ABI 0.3; not frozen)

The repository contains an expanded hosted runtime foundation for compiler development. This is an in-progress production-hardening effort, not yet a production-certified Nexium 1.0 runtime.

### Implemented runtime surface

- Aligned allocation/deallocation with live-byte, peak-byte, and allocation counters.
- Owning byte strings with copy/move operations, UTF-8 validation, and strict Unicode scalar encode/decode.
- Dynamically-sized arrays with overflow checks and safe growth when insertion arguments alias existing elements.
- Console output and fatal panic reporting.
- Binary file open/read/write/flush/close, existence checks, removal, directory creation/query, file copy/move, and file-size queries.
- Monotonic nanosecond clock and millisecond sleep.
- Environment-variable retrieval with required-buffer-size reporting.
- Basic threads (start/join/detach), mutexes, and a bounded worker-pool task scheduler with completion/failure states.
- C-facing ABI for versioning, memory, output, panic, file operations, timing, structured errors, filesystem utilities, process control, TCP sockets, secure randomness, and task scheduling.
- CMake build, active checks in Release tests, Windows/Linux CI, and Linux AddressSanitizer/UndefinedBehaviorSanitizer coverage.

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

- The ABI is currently version 0.3 and is still evolving; it is not yet frozen for long-term binary compatibility.
- Strings store bytes. UTF-8 validity and Unicode scalar encoding/decoding are supported, but normalization, case folding, grapheme segmentation, and locale-aware text are not provided.
- UTF-8 paths are converted to native filesystem paths for the filesystem utility APIs; Windows file open/remove use wide Win32 paths. Atomic durability guarantees and richer metadata/permission APIs are not yet provided.
- Thread callbacks must not rely on exceptions crossing the runtime boundary; uncaught callback exceptions are contained by the thread trampoline.
- Mutexes must be destroyed only after they are unlocked and no thread is using them.
- The runtime is hosted: it uses C++17, the host C runtime, host threading, and native OS services. It is not a freestanding/kernel runtime.
- TCP client and server stream operations are implemented. UDP, TLS, certificate validation, secure HTTP, and a complete networking stack are not implemented.
- Secure random generation is provided, but a complete cryptographic API (hashing, authenticated encryption, key management, and TLS) is not yet implemented. Do not implement cryptographic algorithms in application code as a substitute.
- Process support currently covers launch, wait, terminate, and close; pipes, redirected standard streams, environment/cwd configuration, job objects, and process groups remain future work.
- Allocator replacement hooks, complete Unicode normalization/case-folding/grapheme handling, and a stable long-term ABI remain future work.
- OS-specific permission, metadata, symlink, watcher, and durable atomic-write APIs remain future work.

The implementation in this repository is original Nexium project code. C++17 is used as a bootstrap implementation tool; this does not mean the Nexium compiler or runtime has been implemented by copying another language's runtime.
