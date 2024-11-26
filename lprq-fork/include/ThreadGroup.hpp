#pragma once

#include <thread>
#include <vector>
#include <utility>


class ThreadGroup {
public:
    ThreadGroup() = default;

    ThreadGroup(const ThreadGroup&) = delete;

    ThreadGroup(ThreadGroup&&) = default;

    ThreadGroup& operator=(const ThreadGroup&) = delete;

    ThreadGroup& operator=(ThreadGroup&&) = default;

    template<class T, class... Args>
    void thread(T func, Args&&... args) {
        int tid = static_cast<int>(threads.size());
        threads.emplace_back(std::move(func), std::forward<Args>(args)..., tid);
    }

    template<class T, class R, class... Args>
    void threadWithResult(T func, R& result, Args&&... args) {
        thread([func = std::move(func), &result](Args&&... args, const int tid) {
            result = std::move(func)(std::forward<Args>(args)..., tid);
        }, std::forward<Args>(args)...);
    }

    void join() {
        for (std::thread& thread: threads) {
            thread.join();
        }
        threads.clear();
    }

    ~ThreadGroup() noexcept {
        join();
    }

private:
    std::vector<std::thread> threads{};
};
