#pragma once
#include <functional>
#include <thread>
#include <vector>
#include "BoundedBuffer.hpp"

class ThreadPool {
private:
    BoundedBuffer<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

public:
    ThreadPool(size_t num_threads, size_t max_queue_size) : tasks_(max_queue_size) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    auto task = tasks_.pop();
                    if (!task.has_value()) break;
                    (*task)();
                }
            });
        }
    }

    bool submit(std::function<void()> task) { return tasks_.push(std::move(task)); }

    // RAII: close queue, join every worker
    ~ThreadPool() {
        tasks_.close();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};
