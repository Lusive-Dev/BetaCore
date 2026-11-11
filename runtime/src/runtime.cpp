#include "nexium/runtime.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <mutex>
#include <new>

namespace nexium::runtime {
namespace {
std::atomic<bool> initialized{false};
std::atomic<std::uint64_t> live_bytes{0}, peak_bytes{0}, live_allocations{0};
struct AllocationHeader { void* base; std::size_t size; };
bool valid_alignment(std::size_t a) noexcept { return a!=0 && (a&(a-1))==0; }
const char* mode_string(FileMode mode) noexcept {
    switch(mode) {
        case FileMode::read: return "rb";
        case FileMode::write: return "wb";
        case FileMode::append: return "ab";
        case FileMode::read_write: return "r+b";
    }
    return nullptr;
}
}
bool initialize() noexcept { bool expected=false; return initialized.compare_exchange_strong(expected,true) || expected; }
void shutdown() noexcept { initialized.store(false); }
bool is_initialized() noexcept { return initialized.load(); }

void* allocate(std::size_t size,std::size_t alignment) noexcept {
    if(!valid_alignment(alignment)) return nullptr;
    if(size==0) size=1;
    const std::size_t effective=alignment<alignof(void*)?alignof(void*):alignment;
    const auto max=std::numeric_limits<std::size_t>::max();
    if(effective>max-sizeof(AllocationHeader) || size>max-effective-sizeof(AllocationHeader)) return nullptr;
    const std::size_t total=size+effective-1+sizeof(AllocationHeader);
    void* base=std::malloc(total);
    if(!base) return nullptr;
    const auto start=reinterpret_cast<std::uintptr_t>(base)+sizeof(AllocationHeader);
    const auto aligned=(start+effective-1)&~(static_cast<std::uintptr_t>(effective)-1);
    auto* header=reinterpret_cast<AllocationHeader*>(aligned-sizeof(AllocationHeader));
    header->base=base; header->size=size;
    const auto current=live_bytes.fetch_add(size)+size;
    live_allocations.fetch_add(1);
    auto peak=peak_bytes.load();
    while(current>peak && !peak_bytes.compare_exchange_weak(peak,current)) {}
    return reinterpret_cast<void*>(aligned);
}
void deallocate(void* p,std::size_t) noexcept {
    if(!p) return;
    auto* header=reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uintptr_t>(p)-sizeof(AllocationHeader));
    void* base=header->base; const std::size_t size=header->size;
    live_bytes.fetch_sub(size); live_allocations.fetch_sub(1); std::free(base);
}
MemoryStats memory_stats() noexcept { return {live_bytes.load(),peak_bytes.load(),live_allocations.load()}; }

bool write(const char* text,std::size_t length) noexcept {
    if(!text && length) return false;
    return std::fwrite(text,1,length,stdout)==length;
}
bool write_line(const char* text) noexcept {
    if(!text) return false;
    return write(text,std::strlen(text)) && write("\n",1);
}
[[noreturn]] void panic(const char* message) noexcept {
    constexpr char prefix[]="Nexium runtime panic: ";
    std::fwrite(prefix,1,sizeof(prefix)-1,stderr);
    if(message) std::fwrite(message,1,std::strlen(message),stderr);
    std::fwrite("\n",1,1,stderr); std::fflush(stderr); std::abort();
}

bool valid_utf8(const char* s,std::size_t n) noexcept {
    if(!s && n) return false;
    std::size_t i=0;
    while(i<n) {
        const auto c=static_cast<unsigned char>(s[i]);
        if(c<=0x7f) { ++i; continue; }
        if(c>=0xc2 && c<=0xdf) {
            if(i+1>=n || (static_cast<unsigned char>(s[i+1])&0xc0)!=0x80) return false;
            i+=2; continue;
        }
        if(c>=0xe0 && c<=0xef) {
            if(i+2>=n) return false;
            const auto b1=static_cast<unsigned char>(s[i+1]), b2=static_cast<unsigned char>(s[i+2]);
            if((b1&0xc0)!=0x80 || (b2&0xc0)!=0x80) return false;
            if((c==0xe0 && b1<0xa0) || (c==0xed && b1>=0xa0)) return false;
            i+=3; continue;
        }
        if(c>=0xf0 && c<=0xf4) {
            if(i+3>=n) return false;
            const auto b1=static_cast<unsigned char>(s[i+1]), b2=static_cast<unsigned char>(s[i+2]), b3=static_cast<unsigned char>(s[i+3]);
            if((b1&0xc0)!=0x80 || (b2&0xc0)!=0x80 || (b3&0xc0)!=0x80) return false;
            if((c==0xf0 && b1<0x90) || (c==0xf4 && b1>=0x90)) return false;
            i+=4; continue;
        }
        return false;
    }
    return true;
}

bool file_open(File& file,const char* path,FileMode mode) noexcept {
    if(file.handle || !path || !*path) return false;
    const char* m=mode_string(mode);
    if(!m) return false;
    file.handle=std::fopen(path,m);
    return file.handle!=nullptr;
}
std::size_t file_read(File& file,void* dst,std::size_t bytes) noexcept {
    if(!file.handle || (!dst && bytes)) return 0;
    return std::fread(dst,1,bytes,static_cast<std::FILE*>(file.handle));
}
std::size_t file_write(File& file,const void* src,std::size_t bytes) noexcept {
    if(!file.handle || (!src && bytes)) return 0;
    return std::fwrite(src,1,bytes,static_cast<std::FILE*>(file.handle));
}
bool file_flush(File& file) noexcept { return file.handle && std::fflush(static_cast<std::FILE*>(file.handle))==0; }
bool file_close(File& file) noexcept {
    if(!file.handle) return true;
    auto* stream=static_cast<std::FILE*>(file.handle);
    file.handle=nullptr;
    return std::fclose(stream)==0;
}
bool file_exists(const char* path) noexcept {
    if(!path || !*path) return false;
    std::FILE* f=std::fopen(path,"rb");
    if(!f) return false;
    std::fclose(f); return true;
}
bool file_remove(const char* path) noexcept { return path && std::remove(path)==0; }

std::uint64_t monotonic_time_ns() noexcept {
    const auto value=std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(value).count());
}
void sleep_ms(std::uint64_t ms) noexcept {
    using Rep=std::chrono::milliseconds::rep;
    const auto max_ms=static_cast<std::uint64_t>(std::numeric_limits<Rep>::max());
    while(ms) {
        const auto part=ms>max_ms?max_ms:ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<Rep>(part)));
        ms-=part;
    }
}
namespace {
struct ThreadImpl { std::thread worker; };
}
bool thread_start(Thread& thread,ThreadFunction function,void* data) noexcept {
    if(thread.handle || !function) return false;
    auto* impl=new(std::nothrow) ThreadImpl{};
    if(!impl) return false;
    try { impl->worker=std::thread(function,data); }
    catch(...) { delete impl; return false; }
    thread.handle=impl; return true;
}
bool thread_join(Thread& thread) noexcept {
    if(!thread.handle) return false;
    auto* impl=static_cast<ThreadImpl*>(thread.handle);
    try { if(impl->worker.joinable()) impl->worker.join(); }
    catch(...) { return false; }
    delete impl; thread.handle=nullptr; return true;
}
bool thread_detach(Thread& thread) noexcept {
    if(!thread.handle) return false;
    auto* impl=static_cast<ThreadImpl*>(thread.handle);
    try { if(impl->worker.joinable()) impl->worker.detach(); }
    catch(...) { return false; }
    delete impl; thread.handle=nullptr; return true;
}
bool mutex_initialize(Mutex& mutex) noexcept {
    if(mutex.handle) return false;
    mutex.handle=new(std::nothrow) std::mutex();
    return mutex.handle!=nullptr;
}
void mutex_destroy(Mutex& mutex) noexcept {
    if(!mutex.handle) return;
    delete static_cast<std::mutex*>(mutex.handle); mutex.handle=nullptr;
}
bool mutex_lock(Mutex& mutex) noexcept {
    if(!mutex.handle) return false;
    try { static_cast<std::mutex*>(mutex.handle)->lock(); return true; }
    catch(...) { return false; }
}
bool mutex_unlock(Mutex& mutex) noexcept {
    if(!mutex.handle) return false;
    try { static_cast<std::mutex*>(mutex.handle)->unlock(); return true; }
    catch(...) { return false; }
}

bool environment_get(const char* name,char* dst,std::size_t cap,std::size_t* required) noexcept {
    if(required) *required=0;
    if(!name || !*name || (!dst && cap)) return false;
    const char* value=std::getenv(name);
    if(!value) { if(dst && cap) dst[0]='\0'; return false; }
    const std::size_t need=std::strlen(value)+1;
    if(required) *required=need;
    if(!dst || cap<need) { if(dst && cap) dst[0]='\0'; return false; }
    std::memcpy(dst,value,need); return true;
}

String::String(const char* text) noexcept { if(text) assign(text,std::strlen(text)); }
String::String(const String& other) noexcept { assign(other.c_str(),other.size_); }
String::String(String&& other) noexcept : data_(other.data_),size_(other.size_) { other.data_=nullptr; other.size_=0; }
String::~String() { runtime::deallocate(data_,alignof(char)); }
String& String::operator=(const String& other) noexcept { if(this!=&other) assign(other.c_str(),other.size_); return *this; }
String& String::operator=(String&& other) noexcept {
    if(this!=&other) { runtime::deallocate(data_,alignof(char)); data_=other.data_; size_=other.size_; other.data_=nullptr; other.size_=0; }
    return *this;
}
bool String::assign(const char* text,std::size_t length) noexcept {
    if((!text && length) || length==std::numeric_limits<std::size_t>::max()) return false;
    char* next=nullptr;
    if(length) {
        next=static_cast<char*>(runtime::allocate(length+1,alignof(char)));
        if(!next) return false;
        std::memcpy(next,text,length); next[length]='\0';
    }
    runtime::deallocate(data_,alignof(char)); data_=next; size_=length; return true;
}
bool String::append(const char* text,std::size_t length) noexcept {
    if(!text && length) return false;
    if(!length) return true;
    if(size_>=std::numeric_limits<std::size_t>::max()-length) return false;
    char* next=static_cast<char*>(runtime::allocate(size_+length+1,alignof(char)));
    if(!next) return false;
    if(size_) std::memcpy(next,data_,size_);
    std::memcpy(next+size_,text,length); next[size_+length]='\0';
    runtime::deallocate(data_,alignof(char)); data_=next; size_+=length; return true;
}
bool String::append(const String& other) noexcept { return append(other.c_str(),other.size_); }
void String::clear() noexcept { runtime::deallocate(data_,alignof(char)); data_=nullptr; size_=0; }
const char* String::c_str() const noexcept { return data_?data_:""; }
std::size_t String::size() const noexcept { return size_; }
bool String::empty() const noexcept { return size_==0; }
bool String::equals(const String& other) const noexcept { return size_==other.size_ && (!size_ || std::memcmp(c_str(),other.c_str(),size_)==0); }
bool String::is_valid_utf8() const noexcept { return valid_utf8(c_str(),size_); }
} // namespace nexium::runtime

extern "C" {
std::uint32_t nexium_rt_abi_version() noexcept { return (nexium::runtime::abi_major<<16)|nexium::runtime::abi_minor; }
void* nexium_rt_alloc(std::size_t n,std::size_t a) noexcept { return nexium::runtime::allocate(n,a); }
void nexium_rt_free(void* p,std::size_t a) noexcept { nexium::runtime::deallocate(p,a); }
int nexium_rt_write(const char* s,std::size_t n) noexcept { return nexium::runtime::write(s,n)?1:0; }
[[noreturn]] void nexium_rt_panic(const char* message) noexcept { nexium::runtime::panic(message); }
}
