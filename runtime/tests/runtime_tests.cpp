#include "nexium/runtime.hpp"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)) { std::fprintf(stderr, "CHECK FAILED: %s (%s:%d)\\n", #condition, __FILE__, __LINE__); std::abort(); } } while(0)

using namespace nexium::runtime;
namespace {
void increment_worker(void* data) {
    auto* value=static_cast<std::atomic<int>*>(data);
    value->fetch_add(1,std::memory_order_relaxed);
}
}
int main() {
    assert(initialize() && is_initialized());
    const auto before=memory_stats();
    void* p=allocate(128,64);
    assert(p && reinterpret_cast<std::uintptr_t>(p)%64==0);
    const auto during=memory_stats();
    assert(during.live_allocations==before.live_allocations+1);
    assert(during.live_bytes==before.live_bytes+128);
    deallocate(p,64);
    assert(memory_stats().live_bytes==before.live_bytes);
    assert(allocate(32,3)==nullptr);

    {
        String text("Hello");
        assert(text.size()==5 && text.append(", Nexium",8));
        assert(std::strcmp(text.c_str(),"Hello, Nexium")==0);
        String copy=text;
        assert(copy.equals(text));
        String moved=std::move(copy);
        assert(moved.equals(text));
        assert(text.append(text.c_str(),text.size()));
        assert(std::strcmp(text.c_str(),"Hello, NexiumHello, Nexium")==0);
        const char utf8[]="\xF0\x9F\x8C\xB3";
        assert(valid_utf8(utf8,sizeof(utf8)-1));
        const char invalid[]="\xC0\xAF";
        assert(!valid_utf8(invalid,sizeof(invalid)-1));
        assert(!valid_utf8(nullptr,1));
    }

    {
        Array<int> values;
        for(int i=0;i<100;++i) assert(values.push_back(i));
        assert(values.size()==100 && values[0]==0 && values[99]==99);
        assert(values.at(100)==nullptr);
        values.pop_back();
        assert(values.size()==99);
        Array<int> alias;
        for(int i=0;i<8;++i) assert(alias.push_back(i));
        assert(alias.push_back(alias[0])); // forces growth with an aliased argument
        assert(alias.size()==9 && alias[8]==0);
    }

    {
        const char* path="nexium-runtime-test.tmp";
        File file;
        assert(file_open(file,path,FileMode::write));
        const char payload[]="runtime-file-check";
        assert(file_write(file,payload,sizeof(payload)-1)==sizeof(payload)-1);
        assert(file_flush(file));
        assert(file_close(file));
        assert(file_exists(path));
        assert(file_open(file,path,FileMode::read));
        char buffer[32]{};
        const auto count=file_read(file,buffer,sizeof(buffer));
        assert(count==sizeof(payload)-1);
        assert(std::memcmp(buffer,payload,count)==0);
        assert(file_close(file));
        assert(file_remove(path));
        assert(!file_exists(path));
    }

    {
        NexiumRuntimeFile file{nullptr};
        const char* path="nexium-runtime-cabi.tmp";
        assert(nexium_rt_file_open(&file,path,1));
        const char payload[]="C ABI";
        assert(nexium_rt_file_write(&file,payload,sizeof(payload)-1)==sizeof(payload)-1);
        assert(nexium_rt_file_flush(&file));
        assert(nexium_rt_file_close(&file));
        assert(nexium::runtime::file_remove(path));
        assert((nexium_rt_abi_version()&0xffffu)==abi_minor);
    }

    {
        std::atomic<int> count{0};
        Thread worker;
        assert(thread_start(worker,increment_worker,&count));
        assert(thread_join(worker));
        assert(count.load()==1);
        Mutex mutex;
        assert(mutex_initialize(mutex));
        assert(mutex_lock(mutex));
        assert(mutex_unlock(mutex));
        mutex_destroy(mutex);
        assert(!mutex_lock(mutex));
    }

    {
        const char* dir="nexium-runtime-test-dir/nested";
        assert(directory_create(dir));
        assert(directory_exists(dir));
        const char* source="nexium-runtime-source.tmp";
        const char* copy="nexium-runtime-copy.tmp";
        const char* moved="nexium-runtime-moved.tmp";
        File f;
        assert(file_open(f,source,FileMode::write));
        const char payload[]="filesystem";
        assert(file_write(f,payload,sizeof(payload)-1)==sizeof(payload)-1);
        assert(file_close(f));
        std::uint64_t bytes=0;
        assert(file_size(source,&bytes) && bytes==sizeof(payload)-1);
        assert(file_copy(source,copy,false));
        assert(file_move(copy,moved,false));
        assert(file_remove(source) && file_remove(moved));
        std::remove("nexium-runtime-test-dir/nested");
        std::remove("nexium-runtime-test-dir");
    }

    {
        unsigned char random_bytes[48]{};
        assert(secure_random(random_bytes,sizeof(random_bytes)));
        bool nonzero=false;
        for(unsigned char b:random_bytes) nonzero=nonzero || b!=0;
        assert(nonzero);
    }

    {
#ifdef _WIN32
        const char* executable="cmd.exe";
        const char* args[]={"/c","exit 7"};
#else
        const char* executable="/bin/sh";
        const char* args[]={"-c","exit 7"};
#endif
        Process process;
        assert(process_start(process,executable,args,2));
        std::int32_t exit_code=-1;
        assert(process_wait(process,5000,&exit_code));
        assert(exit_code==7);
        process_close(process);
        assert(process.handle==nullptr);
    }

    {
        assert(network_initialize());
        Socket socket;
        assert(socket.handle==-1);
        tcp_close(socket);
        network_shutdown();
    }

    const auto t1=monotonic_time_ns();
    sleep_ms(1);
    const auto t2=monotonic_time_ns();
    assert(t2>=t1);
    assert(nexium_rt_time_ns()>0);
    assert(write_line("Nexium runtime tests passed."));
    shutdown();
    assert(!is_initialized());
    return 0;
}
