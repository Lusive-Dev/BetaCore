#include "nexium/runtime.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace nexium::runtime {
namespace {
struct TaskImpl {
    std::atomic<TaskState> state{TaskState::queued};
    std::mutex mutex;
    std::condition_variable changed;
    TaskFunction function{nullptr};
    void* user_data{nullptr};
};
struct PoolImpl {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::shared_ptr<TaskImpl>> queue;
    std::vector<std::thread> workers;
    bool stopping{false};
    void worker() noexcept {
        for(;;) {
            std::shared_ptr<TaskImpl> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,[this]{ return stopping || !queue.empty(); });
                if(stopping && queue.empty()) return;
                task=std::move(queue.front()); queue.pop_front();
            }
            task->state.store(TaskState::running,std::memory_order_release);
            TaskState result=TaskState::completed;
            try { task->function(task->user_data); }
            catch(...) { result=TaskState::failed; }
            task->state.store(result,std::memory_order_release);
            task->changed.notify_all();
        }
    }
};
using TaskRef=std::shared_ptr<TaskImpl>;
}
bool task_pool_initialize(TaskPool& pool,std::size_t count) noexcept {
    if(pool.handle || count==0 || count>256) return false;
    auto* impl=new(std::nothrow) PoolImpl();
    if(!impl) return false;
    try {
        impl->workers.reserve(count);
        for(std::size_t i=0;i<count;++i) impl->workers.emplace_back([impl]{ impl->worker(); });
    } catch(...) {
        { std::lock_guard<std::mutex> lock(impl->mutex); impl->stopping=true; }
        impl->ready.notify_all();
        for(auto& worker:impl->workers) if(worker.joinable()) worker.join();
        delete impl; return false;
    }
    pool.handle=impl; return true;
}
bool task_submit(TaskPool& pool,Task& task,TaskFunction function,void* data) noexcept {
    if(!pool.handle || task.handle || !function) return false;
    auto* impl=static_cast<PoolImpl*>(pool.handle);
    std::shared_ptr<TaskImpl> state;
    try {
        state=std::make_shared<TaskImpl>();
        state->function=function; state->user_data=data;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if(impl->stopping) return false;
            impl->queue.push_back(state);
        }
        task.handle=new(std::nothrow) TaskRef(state);
        if(!task.handle) {
            // The queued work remains valid and is allowed to complete.
            return false;
        }
    } catch(...) { return false; }
    impl->ready.notify_one(); return true;
}
TaskState task_state(const Task& task) noexcept {
    if(!task.handle) return TaskState::invalid;
    return (*static_cast<const TaskRef*>(task.handle))->state.load(std::memory_order_acquire);
}
bool task_wait(const Task& task,std::uint64_t timeout_ms) noexcept {
    if(!task.handle) return false;
    const auto state=*static_cast<const TaskRef*>(task.handle);
    std::unique_lock<std::mutex> lock(state->mutex);
    const auto finished=[&]{ auto s=state->state.load(std::memory_order_acquire); return s==TaskState::completed || s==TaskState::failed; };
    if(timeout_ms==std::numeric_limits<std::uint64_t>::max()) {
        state->changed.wait(lock,finished); return true;
    }
    const auto max_ms=static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const auto bounded=timeout_ms>max_ms?max_ms:timeout_ms;
    return state->changed.wait_for(lock,std::chrono::milliseconds(bounded),finished);
}
void task_release(Task& task) noexcept {
    if(!task.handle) return;
    delete static_cast<TaskRef*>(task.handle); task.handle=nullptr;
}
void task_pool_shutdown(TaskPool& pool) noexcept {
    if(!pool.handle) return;
    auto* impl=static_cast<PoolImpl*>(pool.handle);
    { std::lock_guard<std::mutex> lock(impl->mutex); impl->stopping=true; }
    impl->ready.notify_all();
    for(auto& worker:impl->workers) if(worker.joinable()) worker.join();
    delete impl; pool.handle=nullptr;
}
} // namespace nexium::runtime
