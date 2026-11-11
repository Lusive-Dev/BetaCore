#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace nexium::runtime {
inline constexpr std::uint32_t abi_major = 0;
inline constexpr std::uint32_t abi_minor = 2;

enum class ErrorCode : std::uint32_t { none=0, invalid_argument, out_of_memory, out_of_range, io_failure, invalid_state, end_of_file, timeout, not_found, permission_denied, network_failure, process_failure, unsupported, unknown };
struct Error {
    ErrorCode code{ErrorCode::none};
    const char* message{"ok"};
    explicit operator bool() const noexcept { return code != ErrorCode::none; }
};
Error last_error() noexcept;
void clear_error() noexcept;
struct MemoryStats { std::uint64_t live_bytes; std::uint64_t peak_bytes; std::uint64_t live_allocations; };

bool initialize() noexcept;
void shutdown() noexcept;
bool is_initialized() noexcept;
void* allocate(std::size_t size, std::size_t alignment=alignof(std::max_align_t)) noexcept;
void deallocate(void* pointer, std::size_t alignment=alignof(std::max_align_t)) noexcept;
MemoryStats memory_stats() noexcept;

bool write(const char* text, std::size_t length) noexcept;
bool write_line(const char* text) noexcept;
[[noreturn]] void panic(const char* message) noexcept;
bool valid_utf8(const char* text, std::size_t length) noexcept;
bool utf8_decode_next(const char* text, std::size_t length, std::size_t* offset, std::uint32_t* codepoint) noexcept;
bool utf8_encode(std::uint32_t codepoint, char output[4], std::size_t* written) noexcept;

struct File { void* handle{nullptr}; };
enum class FileMode : std::uint8_t { read, write, append, read_write };
bool file_open(File& file, const char* path, FileMode mode) noexcept;
std::size_t file_read(File& file, void* destination, std::size_t bytes) noexcept;
std::size_t file_write(File& file, const void* source, std::size_t bytes) noexcept;
bool file_flush(File& file) noexcept;
bool file_close(File& file) noexcept;
bool file_exists(const char* path) noexcept;
bool file_remove(const char* path) noexcept;
bool directory_create(const char* path) noexcept;
bool directory_exists(const char* path) noexcept;
bool file_copy(const char* source, const char* destination, bool overwrite) noexcept;
bool file_move(const char* source, const char* destination, bool overwrite) noexcept;
bool file_size(const char* path, std::uint64_t* size) noexcept;

struct Process { void* handle{nullptr}; std::uint32_t id{0}; };
bool process_start(Process& process, const char* executable, const char* const* arguments, std::size_t argument_count) noexcept;
bool process_wait(Process& process, std::uint64_t timeout_ms, std::int32_t* exit_code) noexcept;
bool process_terminate(Process& process, std::uint32_t exit_code) noexcept;
void process_close(Process& process) noexcept;

struct Socket { std::intptr_t handle{-1}; };
bool network_initialize() noexcept;
void network_shutdown() noexcept;
bool tcp_connect(Socket& socket, const char* host, std::uint16_t port, std::uint64_t timeout_ms) noexcept;
std::int64_t tcp_send(Socket& socket, const void* data, std::size_t size) noexcept;
std::int64_t tcp_receive(Socket& socket, void* data, std::size_t capacity) noexcept;
void tcp_close(Socket& socket) noexcept;
bool secure_random(void* destination, std::size_t size) noexcept;

std::uint64_t monotonic_time_ns() noexcept;
void sleep_ms(std::uint64_t milliseconds) noexcept;
using ThreadFunction = void(*)(void*);
struct Thread { void* handle{nullptr}; };
bool thread_start(Thread& thread, ThreadFunction function, void* user_data) noexcept;
bool thread_join(Thread& thread) noexcept;
bool thread_detach(Thread& thread) noexcept;
struct Mutex { void* handle{nullptr}; };
bool mutex_initialize(Mutex& mutex) noexcept;
void mutex_destroy(Mutex& mutex) noexcept;
bool mutex_lock(Mutex& mutex) noexcept;
bool mutex_unlock(Mutex& mutex) noexcept;
bool environment_get(const char* name, char* destination, std::size_t capacity, std::size_t* required) noexcept;

class String {
public:
    String() noexcept=default;
    explicit String(const char* text) noexcept;
    String(const String& other) noexcept;
    String(String&& other) noexcept;
    ~String();
    String& operator=(const String& other) noexcept;
    String& operator=(String&& other) noexcept;
    bool assign(const char* text, std::size_t length) noexcept;
    bool append(const char* text, std::size_t length) noexcept;
    bool append(const String& other) noexcept;
    void clear() noexcept;
    const char* c_str() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;
    bool equals(const String& other) const noexcept;
    bool is_valid_utf8() const noexcept;
private:
    char* data_{nullptr};
    std::size_t size_{0};
};

template<class T>
class Array {
    static_assert(std::is_nothrow_destructible<T>::value, "Array elements must have noexcept destructors");
    static_assert(std::is_nothrow_move_constructible<T>::value || std::is_copy_constructible<T>::value,
                  "Array elements must be movable or copyable");
public:
    Array() noexcept=default;
    Array(const Array&)=delete;
    Array& operator=(const Array&)=delete;
    Array(Array&& other) noexcept : data_(other.data_),size_(other.size_),capacity_(other.capacity_) {
        other.data_=nullptr; other.size_=other.capacity_=0;
    }
    Array& operator=(Array&& other) noexcept {
        if(this!=&other) { clear(); runtime::deallocate(data_,alignof(T)); data_=other.data_; size_=other.size_; capacity_=other.capacity_; other.data_=nullptr; other.size_=other.capacity_=0; }
        return *this;
    }
    ~Array() { clear(); runtime::deallocate(data_,alignof(T)); }
    bool reserve(std::size_t requested) noexcept {
        if(requested<=capacity_) return true;
        if(requested>std::numeric_limits<std::size_t>::max()/sizeof(T)) return false;
        T* next=static_cast<T*>(runtime::allocate(requested*sizeof(T),alignof(T)));
        if(!next) return false;
        std::size_t built=0;
        try { for(;built<size_;++built) new(next+built) T(std::move_if_noexcept(data_[built])); }
        catch(...) { while(built) next[--built].~T(); runtime::deallocate(next,alignof(T)); return false; }
        for(std::size_t i=0;i<size_;++i) data_[i].~T();
        runtime::deallocate(data_,alignof(T)); data_=next; capacity_=requested; return true;
    }
    template<class... Args> bool emplace_back(Args&&... args) noexcept {
        try {
            // Construct first so arguments may safely refer to existing elements,
            // even when growing the array reallocates its storage.
            T temporary(std::forward<Args>(args)...);
            if(size_==capacity_) {
                if(capacity_>std::numeric_limits<std::size_t>::max()/2) return false;
                const std::size_t next=capacity_==0?8:capacity_*2;
                if(!reserve(next)) return false;
            }
            new(data_+size_) T(std::move_if_noexcept(temporary));
            ++size_;
            return true;
        } catch(...) { return false; }
    }
    bool push_back(const T& v) noexcept { return emplace_back(v); }
    bool push_back(T&& v) noexcept { return emplace_back(std::move(v)); }
    void pop_back() noexcept { if(size_) data_[--size_].~T(); }
    void clear() noexcept { while(size_) data_[--size_].~T(); }
    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_==0; }
    T& operator[](std::size_t i) noexcept { return data_[i]; }
    const T& operator[](std::size_t i) const noexcept { return data_[i]; }
    T* at(std::size_t i) noexcept { return i<size_?data_+i:nullptr; }
    const T* at(std::size_t i) const noexcept { return i<size_?data_+i:nullptr; }
private:
    T* data_{nullptr}; std::size_t size_{0}; std::size_t capacity_{0};
};
} // namespace nexium::runtime

// Stable C-facing bootstrap ABI. Handles are opaque to Nexium programs.
extern "C" {
struct NexiumRuntimeFile { void* handle; };
std::uint32_t nexium_rt_abi_version() noexcept;
void* nexium_rt_alloc(std::size_t size, std::size_t alignment) noexcept;
void nexium_rt_free(void* pointer, std::size_t alignment) noexcept;
int nexium_rt_write(const char* text, std::size_t length) noexcept;
[[noreturn]] void nexium_rt_panic(const char* message) noexcept;
int nexium_rt_file_open(NexiumRuntimeFile* file, const char* path, std::uint8_t mode) noexcept;
std::size_t nexium_rt_file_read(NexiumRuntimeFile* file, void* destination, std::size_t bytes) noexcept;
std::size_t nexium_rt_file_write(NexiumRuntimeFile* file, const void* source, std::size_t bytes) noexcept;
int nexium_rt_file_flush(NexiumRuntimeFile* file) noexcept;
int nexium_rt_file_close(NexiumRuntimeFile* file) noexcept;
std::uint64_t nexium_rt_time_ns() noexcept;
void nexium_rt_sleep_ms(std::uint64_t milliseconds) noexcept;
}
