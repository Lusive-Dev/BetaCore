#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace nexium::runtime {
inline constexpr std::uint32_t abi_major = 0;
inline constexpr std::uint32_t abi_minor = 1;

enum class ErrorCode : std::uint32_t { none = 0, invalid_argument, out_of_memory, out_of_range, io_failure, invalid_state };
struct Error {
    ErrorCode code{ErrorCode::none};
    const char* message{"ok"};
    explicit operator bool() const noexcept { return code != ErrorCode::none; }
};
struct MemoryStats { std::uint64_t live_bytes; std::uint64_t peak_bytes; std::uint64_t live_allocations; };

bool initialize() noexcept;
void shutdown() noexcept;
bool is_initialized() noexcept;
void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept;
void deallocate(void* pointer, std::size_t alignment = alignof(std::max_align_t)) noexcept;
MemoryStats memory_stats() noexcept;
bool write(const char* text, std::size_t length) noexcept;
bool write_line(const char* text) noexcept;
[[noreturn]] void panic(const char* message) noexcept;

class String {
public:
    String() noexcept = default;
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
private:
    char* data_{nullptr};
    std::size_t size_{0};
};

template<class T>
class Array {
    static_assert(std::is_nothrow_destructible<T>::value, "Array elements must have noexcept destructors");
public:
    Array() noexcept = default;
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    Array(Array&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr; other.size_ = other.capacity_ = 0;
    }
    Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            clear(); runtime::deallocate(data_, alignof(T));
            data_ = other.data_; size_ = other.size_; capacity_ = other.capacity_;
            other.data_ = nullptr; other.size_ = other.capacity_ = 0;
        }
        return *this;
    }
    ~Array() { clear(); runtime::deallocate(data_, alignof(T)); }
    bool reserve(std::size_t requested) noexcept {
        if (requested <= capacity_) return true;
        if (requested > static_cast<std::size_t>(-1) / sizeof(T)) return false;
        void* raw = runtime::allocate(requested * sizeof(T), alignof(T));
        if (!raw) return false;
        T* next = static_cast<T*>(raw);
        std::size_t moved = 0;
        try {
            for (; moved < size_; ++moved) new (next + moved) T(std::move_if_noexcept(data_[moved]));
        } catch (...) {
            while (moved) next[--moved].~T();
            runtime::deallocate(next, alignof(T));
            return false;
        }
        for (std::size_t i = 0; i < size_; ++i) data_[i].~T();
        runtime::deallocate(data_, alignof(T));
        data_ = next; capacity_ = requested;
        return true;
    }
    template<class... Args> bool emplace_back(Args&&... args) noexcept {
        if (size_ == capacity_) {
            const std::size_t next = capacity_ == 0 ? 8 : capacity_ * 2;
            if (next < capacity_ || !reserve(next)) return false;
        }
        try { new (data_ + size_) T(std::forward<Args>(args)...); ++size_; return true; }
        catch (...) { return false; }
    }
    bool push_back(const T& value) noexcept { return emplace_back(value); }
    bool push_back(T&& value) noexcept { return emplace_back(std::move(value)); }
    void pop_back() noexcept { if (size_) data_[--size_].~T(); }
    void clear() noexcept { while (size_) data_[--size_].~T(); }
    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }
    T& operator[](std::size_t i) noexcept { return data_[i]; }
    const T& operator[](std::size_t i) const noexcept { return data_[i]; }
    T* at(std::size_t i) noexcept { return i < size_ ? data_ + i : nullptr; }
    const T* at(std::size_t i) const noexcept { return i < size_ ? data_ + i : nullptr; }
private:
    T* data_{nullptr}; std::size_t size_{0}; std::size_t capacity_{0};
};
} // namespace nexium::runtime
