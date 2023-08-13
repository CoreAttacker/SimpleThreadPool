#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include<mingw.mutex.h>
#include<mingw.condition_variable.h>
#include<mingw.thread.h>
#include<future>
#include<vector>
#include<queue>
#include<iostream>

class ThreadPool{
    private:
        ThreadPool(size_t);
        ThreadPool();
        ~ThreadPool();
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

        void Works_Initialize(){
            for(size_t i = 0;i < works.size();++i)
            {
                auto worker = [this, i]()
                {
                    while(!if_stop)
                    {
                        TaskType task;
                        {
                            std::unique_lock<std::mutex> lock(this->mtx);
                            this->condition.wait(lock, [this]{ return this->if_stop || !this->tasks.empty(); });
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        std::cout << "thread" << i << " starts running" << std::endl;
                        task(); //start executing
                        std::cout << "thread" << i << " ends running" << std::endl;
                    }
                };
            }
        }
    public:
        using TaskType = std::function<void()>;

        std::vector<std::thread> works;
        std::queue<TaskType> tasks;
        std::condition_variable condition;
        std::mutex mtx;
        bool if_stop;
};

inline ThreadPool::ThreadPool():works(std::thread::hardware_concurrency()), if_stop(false)
{
    Works_Initialize();
}

inline ThreadPool::ThreadPool(size_t Thread_Number):works(Thread_Number), if_stop(false)
{
    Works_Initialize();
}

inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(this->mtx);
        this->if_stop = true;
    }
    this->condition.notify_all();
    for(std::thread &i:works)
    {
        i.join();
    }
}

template<class F, class... Args>
inline auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

#endif
