#include "nexium/runtime.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#if defined(_WIN32)
# include <malloc.h>
#endif

namespace nexium::runtime {
namespace {
std::atomic<bool> initialized{false};
std::atomic<std::uint64_t> live_bytes{0}, peak_bytes{0}, live_allocations{0};
bool valid_alignment(std::size_t a) noexcept { return a >= sizeof(void*) && (a & (a - 1)) == 0; }
}
bool initialize() noexcept { bool expected=false; return initialized.compare_exchange_strong(expected,true) || expected; }
void shutdown() noexcept { initialized.store(false); }
bool is_initialized() noexcept { return initialized.load(); }

void* allocate(std::size_t size, std::size_t alignment) noexcept {
    if (!valid_alignment(alignment)) return nullptr;
    if (size == 0) size = 1;
#if defined(_WIN32)
    void* p = _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    if (posix_memalign(&p, alignment, size) != 0) p = nullptr;
#endif
    if (!p) return nullptr;
    const auto current = live_bytes.fetch_add(size) + size;
    live_allocations.fetch_add(1);
    auto peak = peak_bytes.load();
    while (current > peak && !peak_bytes.compare_exchange_weak(peak, current)) {}
    return p;
}
void deallocate(void* p, std::size_t) noexcept {
    if (!p) return;
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
    live_allocations.fetch_sub(1);
}
MemoryStats memory_stats() noexcept { return {live_bytes.load(), peak_bytes.load(), live_allocations.load()}; }

bool write(const char* text, std::size_t length) noexcept {
    if (!text && length) return false;
    return std::fwrite(text, 1, length, stdout) == length;
}
bool write_line(const char* text) noexcept {
    if (!text) return false;
    return write(text, std::strlen(text)) && write("\n", 1);
}
[[noreturn]] void panic(const char* message) noexcept {
    constexpr char prefix[] = "Nexium runtime panic: ";
    std::fwrite(prefix, 1, sizeof(prefix)-1, stderr);
    if (message) std::fwrite(message, 1, std::strlen(message), stderr);
    std::fwrite("\n", 1, 1, stderr);
    std::fflush(stderr);
    std::abort();
}

String::String(const char* text) noexcept { if (text) assign(text, std::strlen(text)); }
String::String(const String& other) noexcept { assign(other.c_str(), other.size_); }
String::String(String&& other) noexcept : data_(other.data_), size_(other.size_) { other.data_=nullptr; other.size_=0; }
String::~String() { runtime::deallocate(data_, alignof(char)); }
String& String::operator=(const String& other) noexcept {
    if (this != &other) assign(other.c_str(), other.size_);
    return *this;
}
String& String::operator=(String&& other) noexcept {
    if (this != &other) {
        runtime::deallocate(data_, alignof(char)); data_=other.data_; size_=other.size_;
        other.data_=nullptr; other.size_=0;
    }
    return *this;
}
bool String::assign(const char* text, std::size_t length) noexcept {
    if (!text && length) return false;
    char* next=nullptr;
    if (length) {
        next=static_cast<char*>(runtime::allocate(length+1, alignof(char)));
        if (!next) return false;
        std::memcpy(next,text,length); next[length]='\0';
    }
    runtime::deallocate(data_,alignof(char)); data_=next; size_=length; return true;
}
bool String::append(const char* text, std::size_t length) noexcept {
    if (!text && length) return false;
    if (!length) return true;
    if (size_ > std::numeric_limits<std::size_t>::max()-length-1) return false;
    char* next=static_cast<char*>(runtime::allocate(size_+length+1,alignof(char)));
    if (!next) return false;
    if (size_) std::memcpy(next,data_,size_);
    std::memcpy(next+size_,text,length); next[size_+length]='\0';
    runtime::deallocate(data_,alignof(char)); data_=next; size_+=length; return true;
}
bool String::append(const String& other) noexcept { return append(other.c_str(),other.size_); }
void String::clear() noexcept { runtime::deallocate(data_,alignof(char)); data_=nullptr; size_=0; }
const char* String::c_str() const noexcept { return data_ ? data_ : ""; }
std::size_t String::size() const noexcept { return size_; }
bool String::empty() const noexcept { return size_==0; }
bool String::equals(const String& other) const noexcept {
    return size_==other.size_ && (!size_ || std::memcmp(c_str(),other.c_str(),size_)==0);
}
} // namespace nexium::runtime
