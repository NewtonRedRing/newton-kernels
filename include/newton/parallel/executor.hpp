#pragma once
#include <algorithm>
#include <exception>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace newton::parallel {
class Executor {
public:
    explicit Executor(std::size_t threads = std::thread::hardware_concurrency());
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    [[nodiscard]] std::size_t size() const noexcept { return workers_.size(); }
    template<class F>
    auto submit(F&& f) -> std::future<std::invoke_result_t<F&>> {
        using Result = std::invoke_result_t<F&>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(f));
        auto result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("Executor stopped");
            queue_.emplace_back([task] { (*task)(); });
        }
        wake_.notify_one();
        return result;
    }
    // func(begin, end) is invoked on disjoint [begin,end) ranges.
    template<class F>
    void parallel_for(std::size_t begin, std::size_t end, std::size_t grain, F&& func) {
        if (grain == 0) throw std::invalid_argument("grain must be positive");
        if (end < begin) throw std::invalid_argument("invalid range");
        std::vector<std::future<void>> jobs;
        for (std::size_t start = begin; start < end;) {
            const auto count = std::min(grain, end - start);
            const auto finish = start + count;
            jobs.push_back(submit([&func, start, finish] { func(start, finish); }));
            start = finish;
        }
        std::exception_ptr first;
        for (auto& job : jobs) {
            try { job.get(); } catch (...) { if (!first) first = std::current_exception(); }
        }
        if (first) std::rethrow_exception(first);
    }
private:
    void worker_loop();
    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
};
} // namespace newton::parallel
