#include "nexium/runtime.hpp"
#include "nexium/stdlib.hpp"
#include <string>
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
struct TcpTestState { Socket* listener; std::atomic<int> received{0}; };
void task_worker(void* data) {
    auto* value=static_cast<std::atomic<int>*>(data);
    value->fetch_add(1,std::memory_order_relaxed);
}
void failing_task(void*) { throw 1; }
void tcp_worker(void* data) {
    auto* state=static_cast<TcpTestState*>(data);
    Socket client;
    if(!tcp_accept(*state->listener,client,5000)) return;
    char buffer[4]{};
    const auto n=tcp_receive(client,buffer,sizeof(buffer));
    if(n==4 && std::memcmp(buffer,"ping",4)==0) state->received.store(1);
    tcp_close(client);
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
        std::size_t offset=0, written=0; std::uint32_t scalar=0; char encoded[4]{};
        assert(utf8_decode_next(utf8,sizeof(utf8)-1,&offset,&scalar));
        assert(scalar==0x1f333 && offset==sizeof(utf8)-1);
        assert(utf8_encode(scalar,encoded,&written) && written==4);
        assert(std::memcmp(encoded,utf8,4)==0);
        assert(!utf8_encode(0xd800,encoded,&written));
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
        const char* unicode_path="nexium-runtime-\xE2\x98\x83.tmp";
        assert(file_open(file,unicode_path,FileMode::write));
        assert(file_write(file,"utf8",4)==4);
        assert(file_close(file));
        assert(file_exists(unicode_path));
        assert(file_remove(unicode_path));
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

        const char* overwrite_source="nexium-runtime-overwrite-source.tmp";
        const char* overwrite_destination="nexium-runtime-overwrite-destination.tmp";
        assert(file_open(f,overwrite_source,FileMode::write));
        assert(file_write(f,"new",3)==3);
        assert(file_close(f));
        assert(file_open(f,overwrite_destination,FileMode::write));
        assert(file_write(f,"old",3)==3);
        assert(file_close(f));
        assert(!file_move(overwrite_source,overwrite_destination,false));
        assert(file_exists(overwrite_source) && file_exists(overwrite_destination));
        assert(file_move(overwrite_source,overwrite_destination,true));
        assert(!file_exists(overwrite_source));
        assert(file_open(f,overwrite_destination,FileMode::read));
        char replacement[4]{};
        assert(file_read(f,replacement,3)==3);
        assert(std::memcmp(replacement,"new",3)==0);
        assert(file_close(f));
        assert(file_remove(overwrite_destination));

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
        NexiumRuntimeTaskPool pool;
        NexiumRuntimeTask task;
        std::atomic<int> value{0};
        assert(nexium_rt_task_pool_initialize(&pool,1));
        assert(nexium_rt_task_submit(&pool,&task,task_worker,&value));
        assert(nexium_rt_task_wait(&task,5000));
        assert(nexium_rt_task_state(&task)==static_cast<std::uint32_t>(TaskState::completed));
        nexium_rt_task_release(&task);
        nexium_rt_task_pool_shutdown(&pool);
        assert(value.load()==1);
        unsigned char bytes[16]{};
        assert(nexium_rt_secure_random(bytes,sizeof(bytes)));
        assert(nexium_rt_last_error_code()==static_cast<std::uint32_t>(ErrorCode::none));
    }

    {
        TaskPool pool;
        assert(task_pool_initialize(pool,3));
        std::atomic<int> completed{0};
        Task tasks[12];
        for(auto& task:tasks) assert(task_submit(pool,task,task_worker,&completed));
        Task failed;
        assert(task_submit(pool,failed,failing_task,nullptr));
        for(auto& task:tasks) {
            assert(task_wait(task,5000));
            assert(task_state(task)==TaskState::completed);
            task_release(task);
        }
        assert(task_wait(failed,5000));
        assert(task_state(failed)==TaskState::failed);
        task_release(failed);
        assert(completed.load()==12);
        task_pool_shutdown(pool);
        assert(pool.handle==nullptr);
    }

    {
        assert(network_initialize());
        Socket listener;
        std::uint16_t port=0;
        assert(tcp_listen(listener,"127.0.0.1",0,&port,8) && port!=0);
        TcpTestState state{&listener};
        Thread server;
        assert(thread_start(server,tcp_worker,&state));
        Socket client;
        assert(tcp_connect(client,"127.0.0.1",port,5000));
        assert(tcp_send(client,"ping",4)==4);
        assert(thread_join(server));
        assert(state.received.load()==1);
        tcp_close(client);
        tcp_close(listener);
        network_shutdown();
    }


    {
        using namespace nexium::stdlib;
        Map<std::string,int> map;
        assert(map.insert_or_assign("answer",42));
        assert(map.contains("answer") && *map.find("answer")==42);
        assert(map.insert_or_assign("answer",43) && *map.find("answer")==43);
        assert(map.erase("answer") && !map.contains("answer"));
        Set<int> set;
        assert(set.insert(4) && !set.insert(4) && set.contains(4));
        Queue<int> queue;
        assert(queue.push(7) && queue.push(9));
        int item=0;
        assert(queue.pop(&item) && item==7);
        Array<int> numbers;
        assert(numbers.push_back(5) && numbers.push_back(1) && numbers.push_back(3));
        assert(sort(numbers) && numbers[0]==1 && numbers[2]==5);
        assert(binary_search(numbers,3) && *binary_search(numbers,3)==3);
        assert(binary_search(numbers,4)==nullptr);
        assert(math::abs(-2.5)==2.5 && math::sqrt(9.0)==3.0);
        std::int64_t integer=0;
        assert(parse_integer("-123",&integer) && integer==-123);
        assert(!parse_integer("12x",&integer));
        double floating=0;
        assert(parse_float("3.25",&floating) && floating==3.25);
        char formatted[64]{};
        std::size_t required=0;
        assert(format_integer(12345,formatted,sizeof(formatted),&required));
        assert(std::strcmp(formatted,"12345")==0 && required==6);
        bool matched=false;
        assert(regex_match("nexium[0-9]+","nexium[0-9]+",&matched) && matched);
        assert(regex_search("hello nexium 42","[0-9]+",&matched) && matched);
        DateTime date;
        assert(datetime_utc(0,&date) && date.year==1970 && date.month==1 && date.day==1);
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
