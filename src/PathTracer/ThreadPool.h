#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool
{
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

	void InitialiseThreads(size_t numThreads);
    void EnqueueTask(std::function<void()> task);
    void WaitForCompletion();
    void Shutdown();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable completionCondition;
    bool stop;
    int activeTasks;
};