#include "nexium/runtime.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <cstdint>

namespace nexium::runtime {
namespace {
std::atomic<bool> initialized{false};
std::atomic<std::uint64_t> live_bytes{0}, peak_bytes{0}, live_allocations{0};
struct AllocationHeader { void* base; std::size_t size; };
bool valid_alignment(std::size_t a) noexcept { return a != 0 && (a & (a - 1)) == 0; }
}
bool initialize() noexcept { bool expected=false; return initialized.compare_exchange_strong(expected,true) || expected; }
void shutdown() noexcept { initialized.store(false); }
bool is_initialized() noexcept { return initialized.load(); }

void* allocate(std::size_t size, std::size_t alignment) noexcept {
    if (!valid_alignment(alignment)) return nullptr;
    if (size == 0) size = 1;
    const std::size_t effective = alignment < alignof(void*) ? alignof(void*) : alignment;
    if (size > std::numeric_limits<std::size_t>::max() - effective - sizeof(AllocationHeader)) return nullptr;
    const std::size_t total = size + effective - 1 + sizeof(AllocationHeader);
    void* base = std::malloc(total);
    if (!base) return nullptr;
    const auto start = reinterpret_cast<std::uintptr_t>(base) + sizeof(AllocationHeader);
    const auto aligned = (start + effective - 1) & ~(static_cast<std::uintptr_t>(effective) - 1);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
    header->base = base;
    header->size = size;
    const auto current = live_bytes.fetch_add(size) + size;
    live_allocations.fetch_add(1);
    auto peak = peak_bytes.load();
    while (current > peak && !peak_bytes.compare_exchange_weak(peak, current)) {}
    return reinterpret_cast<void*>(aligned);
}
void deallocate(void* p, std::size_t) noexcept {
    if (!p) return;
    auto* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uintptr_t>(p) - sizeof(AllocationHeader));
    void* base = header->base;
    const std::size_t size = header->size;
    live_bytes.fetch_sub(size);
    live_allocations.fetch_sub(1);
    std::free(base);
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
    if (length > std::numeric_limits<std::size_t>::max()-size_-1) return false;
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
