// 实现自己的线程池

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <memory>
#include <functional>
#include <stdexcept>


namespace LI {
    class ThreadPool {
    public:
        /// @brief 构造函数
        /// @param threads 要启动的线程个数
        ThreadPool(size_t threads);

        /// @brief 把任务放入任务队列
        /// @tparam _Callable 可调用对象类型
        /// @tparam ...Args 可调用对象类型的参数类型
        /// @param _f 可调用对象
        /// @param ...args 可调用对象的参数
        /// @return future<可调用对象的返回类型>
        template<class _Callable, class... Args>
        auto enqueue(_Callable&& _f, Args&&... args)
            -> std::future< typename std::result_of<_Callable(Args...)>::type >;
        

        // 析构函数
        ~ThreadPool();
    private:
        std::vector<std::thread> workers; // 线程数组
        // 任务队列
        // 任务的可执行函数时无参数无返回类型的可调用对象
        std::queue< std::function<void()> > tasks;

        // 同步变量
        std::mutex queue_mutex;
        std::condition_variable condv;
        bool stop; // 终止标记
    };

    ThreadPool::ThreadPool(size_t threads): stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            // 新增线程
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    // 取任务
                    {
                        std::unique_lock<std::mutex> lk(this->queue_mutex);
                        // 等待锁 且 满足条件变量
                        this->condv.wait(lk, 
                            [this]() {
                                return this->stop || !this->tasks.empty();
                        });
                        // 保证队列为空 且 有终止标记
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        task = std::move(this->tasks.front()); // 避免复制
                        this->tasks.pop();
                        // 出作用域 lk 自动 unlock()
                    }

                    task(); // 执行任务
                }
            } );
        }
    }

    template<class _Callable, class... Args>
    auto ThreadPool::enqueue(_Callable&& _f, Args&&... args)
        -> std::future< typename std::result_of<_Callable(Args...)>::type >
    {
        using return_type = typename std::result_of<_Callable(Args...)>::type;
        
        // 对可调用对象进一步打包, 使其转为 void() 型函数, 使得任务队列满足不同的任务

        // 创建一个 share_ptr 指向 packaged_task(这是一个可调用对象) 对象 
        // 使用 share_ptr 是为了函数结束时, 该指针不被释放
        // packaged_task 对象绑定的是 bind 函数返回的已绑定参数的可调用对象
        // packaged_task 对象无参数, 返回类型是 return_type
        std::shared_ptr<std::packaged_task<return_type()>> 
        task = std::make_shared< std::packaged_task<return_type()> >(
                std::bind(std::forward<_Callable>(_f), std::forward<Args>(args)...)
        );
        
        // 任务的结果
        std::future<return_type> res = task->get_future();
        // std::future<return_type> res = task.get_future();

        {
            // 上锁
            std::unique_lock<std::mutex> lk(queue_mutex);
            // stop 之后不能再添加任务
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            // 这里使用值捕获, 会使 share_ptr 计数 + 1
            // 不能直接使用非指针类型的 task(std::packaged_task对象), 因为该类的 ctor 是 delete 的
            tasks.emplace([task](){ (*task)(); });
        }

        condv.notify_one(); // 唤醒一个worker线程
        return res;
    }

    ThreadPool::~ThreadPool() {
        {
            std::unique_lock<std::mutex> lk(queue_mutex);
            stop = true;
        }
        condv.notify_all(); // 唤醒所有线程
        for (std::thread& worker : workers) {
            worker.join();
        }
    }


}






#endif
