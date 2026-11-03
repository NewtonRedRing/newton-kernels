#include <newton/parallel/executor.hpp>
namespace newton::parallel {
thread_local Executor* Executor::running_on_ = nullptr;

Executor::Executor(std::size_t threads) {
    if (!threads) threads = 1;
    try {
        for (std::size_t i = 0; i < threads; ++i) workers_.emplace_back([this] { worker_loop(); });
    } catch (...) {
        { std::lock_guard lock(mutex_); stop_ = true; }
        wake_.notify_all();
        for (auto& thread : workers_) if (thread.joinable()) thread.join();
        throw;
    }
}
Executor::~Executor() {
    { std::lock_guard lock(mutex_); stop_ = true; }
    wake_.notify_all();
    for (auto& thread : workers_) if (thread.joinable()) thread.join();
}
void Executor::worker_loop() {
    // Restored even if a worker exits; this is per-thread bookkeeping, not
    // a global scheduler or an implicit application runtime.
    struct Restore {
        Executor*& slot;
        Executor* previous;
        ~Restore() noexcept { slot = previous; }
    };
    Restore restore{running_on_, running_on_};
    running_on_ = this;
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();
    }
}
} // namespace newton::parallel
