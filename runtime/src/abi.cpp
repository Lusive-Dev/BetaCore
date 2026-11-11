#include "nexium/runtime.hpp"

extern "C" {
std::uint32_t nexium_rt_last_error_code() noexcept {
    return static_cast<std::uint32_t>(nexium::runtime::last_error().code);
}
const char* nexium_rt_last_error_message() noexcept {
    return nexium::runtime::last_error().message;
}
int nexium_rt_directory_create(const char* path) noexcept { return nexium::runtime::directory_create(path)?1:0; }
int nexium_rt_directory_exists(const char* path) noexcept { return nexium::runtime::directory_exists(path)?1:0; }
int nexium_rt_file_copy(const char* src,const char* dst,int overwrite) noexcept { return nexium::runtime::file_copy(src,dst,overwrite!=0)?1:0; }
int nexium_rt_file_move(const char* src,const char* dst,int overwrite) noexcept { return nexium::runtime::file_move(src,dst,overwrite!=0)?1:0; }
int nexium_rt_file_size(const char* path,std::uint64_t* size) noexcept { return nexium::runtime::file_size(path,size)?1:0; }

int nexium_rt_process_start(NexiumRuntimeProcess* p,const char* exe,const char* const* args,std::size_t count) noexcept {
    if(!p) return 0;
    nexium::runtime::Process value{p->handle,p->id};
    const bool ok=nexium::runtime::process_start(value,exe,args,count);
    p->handle=value.handle; p->id=value.id; return ok?1:0;
}
int nexium_rt_process_wait(NexiumRuntimeProcess* p,std::uint64_t timeout,std::int32_t* code) noexcept {
    if(!p) return 0;
    nexium::runtime::Process value{p->handle,p->id};
    return nexium::runtime::process_wait(value,timeout,code)?1:0;
}
int nexium_rt_process_terminate(NexiumRuntimeProcess* p,std::uint32_t code) noexcept {
    if(!p) return 0;
    nexium::runtime::Process value{p->handle,p->id};
    return nexium::runtime::process_terminate(value,code)?1:0;
}
void nexium_rt_process_close(NexiumRuntimeProcess* p) noexcept {
    if(!p) return;
    nexium::runtime::Process value{p->handle,p->id};
    nexium::runtime::process_close(value); p->handle=value.handle; p->id=value.id;
}
int nexium_rt_network_initialize() noexcept { return nexium::runtime::network_initialize()?1:0; }
void nexium_rt_network_shutdown() noexcept { nexium::runtime::network_shutdown(); }
int nexium_rt_tcp_connect(NexiumRuntimeSocket* s,const char* host,std::uint16_t port,std::uint64_t timeout) noexcept {
    if(!s) return 0;
    nexium::runtime::Socket value{s->handle};
    const bool ok=nexium::runtime::tcp_connect(value,host,port,timeout); s->handle=value.handle; return ok?1:0;
}
int nexium_rt_tcp_listen(NexiumRuntimeSocket* s,const char* host,std::uint16_t port,std::uint16_t* bound,int backlog) noexcept {
    if(!s) return 0;
    nexium::runtime::Socket value{s->handle};
    const bool ok=nexium::runtime::tcp_listen(value,host,port,bound,backlog); s->handle=value.handle; return ok?1:0;
}
int nexium_rt_tcp_accept(NexiumRuntimeSocket* listener,NexiumRuntimeSocket* client,std::uint64_t timeout) noexcept {
    if(!listener || !client) return 0;
    nexium::runtime::Socket a{listener->handle}, b{client->handle};
    const bool ok=nexium::runtime::tcp_accept(a,b,timeout); listener->handle=a.handle; client->handle=b.handle; return ok?1:0;
}
std::int64_t nexium_rt_tcp_send(NexiumRuntimeSocket* s,const void* data,std::size_t size) noexcept {
    if(!s) return -1; nexium::runtime::Socket value{s->handle}; return nexium::runtime::tcp_send(value,data,size);
}
std::int64_t nexium_rt_tcp_receive(NexiumRuntimeSocket* s,void* data,std::size_t size) noexcept {
    if(!s) return -1; nexium::runtime::Socket value{s->handle}; return nexium::runtime::tcp_receive(value,data,size);
}
void nexium_rt_tcp_close(NexiumRuntimeSocket* s) noexcept {
    if(!s) return; nexium::runtime::Socket value{s->handle}; nexium::runtime::tcp_close(value); s->handle=value.handle;
}
int nexium_rt_secure_random(void* destination,std::size_t size) noexcept { return nexium::runtime::secure_random(destination,size)?1:0; }

int nexium_rt_task_pool_initialize(NexiumRuntimeTaskPool* p,std::size_t workers) noexcept {
    if(!p) return 0; nexium::runtime::TaskPool value{p->handle};
    const bool ok=nexium::runtime::task_pool_initialize(value,workers); p->handle=value.handle; return ok?1:0;
}
int nexium_rt_task_submit(NexiumRuntimeTaskPool* p,NexiumRuntimeTask* t,NexiumRuntimeTaskFunction fn,void* data) noexcept {
    if(!p || !t) return 0;
    nexium::runtime::TaskPool pool{p->handle}; nexium::runtime::Task task{t->handle};
    const bool ok=nexium::runtime::task_submit(pool,task,fn,data); t->handle=task.handle; return ok?1:0;
}
std::uint32_t nexium_rt_task_state(const NexiumRuntimeTask* t) noexcept {
    if(!t) return static_cast<std::uint32_t>(nexium::runtime::TaskState::invalid);
    return static_cast<std::uint32_t>(nexium::runtime::task_state(nexium::runtime::Task{t->handle}));
}
int nexium_rt_task_wait(const NexiumRuntimeTask* t,std::uint64_t timeout) noexcept {
    if(!t) return 0; return nexium::runtime::task_wait(nexium::runtime::Task{t->handle},timeout)?1:0;
}
void nexium_rt_task_release(NexiumRuntimeTask* t) noexcept {
    if(!t) return; nexium::runtime::Task task{t->handle}; nexium::runtime::task_release(task); t->handle=task.handle;
}
void nexium_rt_task_pool_shutdown(NexiumRuntimeTaskPool* p) noexcept {
    if(!p) return; nexium::runtime::TaskPool pool{p->handle}; nexium::runtime::task_pool_shutdown(pool); p->handle=pool.handle;
}
}
