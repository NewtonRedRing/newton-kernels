#include <newton/parallel/executor.hpp>
namespace newton::parallel {
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
