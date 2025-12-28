#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount)
        : stop(false), activeTasks(0)
    {
        threadCount = std::max<size_t>(1, threadCount);

        for (size_t i = 0; i < threadCount; ++i)
        {
            workers.emplace_back([this]
                                 {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        cv.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });

                        if (stop && tasks.empty())
                            return;

                        task = std::move(tasks.front());
                        tasks.pop();
                        activeTasks++;
                    }

                    task();

                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        activeTasks--;
                    }
                    done.notify_one();
                } });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto &t : workers)
            t.join();
    }

    inline void enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        done.wait(lock, [this]
                  { return tasks.empty() && activeTasks == 0; });
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable done;

    std::atomic<bool> stop;
    std::atomic<size_t> activeTasks;
};
