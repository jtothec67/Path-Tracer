#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads) : stop(false), activeTasks(0)
{
    InitialiseThreads(numThreads);
}

void ThreadPool::InitialiseThreads(size_t numThreads)
{
    for (size_t i = 0; i < numThreads; ++i)
    {
        // Create a worker thread and add it to the workers vector
        workers.emplace_back([this]
        {
                while (true)
                {
                    std::function<void()> task;

                    {
                        // Lock the queue and wait for a task to be available or for the pool to stop
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });

                        if (stop && tasks.empty())
                            return; // Exit the thread if the pool is stopping and no tasks are left

                        task = std::move(tasks.front()); // Get the next task
                        tasks.pop(); // Remove the task from the queue
                        ++activeTasks; // Increment the count of active tasks
                    }

                    task(); // Execute the task

                    {
                        // Lock the queue and update the active tasks count
                        std::unique_lock<std::mutex> lock(queueMutex);
                        --activeTasks;

                        // Notify all waiting threads if there are no more tasks and no active tasks
                        if (tasks.empty() && activeTasks == 0)
                        {
                            completionCondition.notify_all();
                        }
                    }
                }
        });
    }
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::EnqueueTask(std::function<void()> task)
{
    {
        // Lock the queue to ensure thread safety
        std::unique_lock<std::mutex> lock(queueMutex);
        // Add the new task to the queue
        tasks.emplace(std::move(task));
    }
    // Notify one waiting thread that a new task is available
    condition.notify_one();
}

void ThreadPool::WaitForCompletion()
{
    // Lock the queue to ensure thread safety
    std::unique_lock<std::mutex> lock(queueMutex);
    // Wait until all tasks are completed and there are no active tasks
    completionCondition.wait(lock, [this] { return tasks.empty() && activeTasks == 0; });
}

void ThreadPool::Shutdown()
{
    {
        // Lock the queue to ensure thread safety
        std::unique_lock<std::mutex> lock(queueMutex);
        // Set the stop flag to true to signal all threads to stop
        stop = true;
    }

    // Notify all waiting threads that the pool is stopping
    condition.notify_all();
    // Join all worker threads to ensure they have finished executing
    for (std::thread& worker : workers)
        worker.join();
    // Clear the workers vector
    workers.clear();
    // Reset the stop flag to false
    stop = false;
}