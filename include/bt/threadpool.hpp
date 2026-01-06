/**
 * @file threadpool.hpp
 * @brief 线程池框架 - 用于并行策略优化
 * 
 * 对应 Python backtrader 的 multiprocessing.Pool
 * 提供任务并行执行能力，支持策略参数优化。
 * 
 * 特性:
 * - 固定大小线程池
 * - 任务队列
 * - Future/Promise 模式
 * - 支持批量任务提交
 * - 优雅关闭
 */

#pragma once

#include "bt/common.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <atomic>
#include <memory>
#include <type_traits>

namespace bt {

/**
 * @brief 线程池类
 * 
 * 使用固定数量的工作线程处理任务队列中的任务
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param numThreads 工作线程数量，默认为硬件线程数
     */
    explicit ThreadPool(Size numThreads = 0)
        : stop_(false), activeJobs_(0) {
        
        Size threads = (numThreads > 0) ? numThreads : 
                       static_cast<Size>(std::thread::hardware_concurrency());
        if (threads == 0) threads = 1;  // 至少一个线程
        
        workers_.reserve(threads);
        for (Size i = 0; i < threads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }
    
    /**
     * @brief 析构函数 - 等待所有任务完成并关闭线程
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // 禁用拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    /**
     * @brief 提交任务到线程池
     * @tparam F 可调用类型
     * @tparam Args 参数类型
     * @param f 任务函数
     * @param args 参数
     * @return std::future 用于获取结果
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        
        using ReturnType = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool has been stopped");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    /**
     * @brief 批量提交任务
     * @tparam F 可调用类型
     * @tparam Container 参数容器类型
     * @param f 任务函数
     * @param argsContainer 参数容器
     * @return vector of futures
     */
    template<typename F, typename Container>
    auto submitBatch(F&& f, const Container& argsContainer)
        -> std::vector<std::future<typename std::invoke_result<F, typename Container::value_type>::type>> {
        
        using ReturnType = typename std::invoke_result<F, typename Container::value_type>::type;
        
        std::vector<std::future<ReturnType>> futures;
        futures.reserve(argsContainer.size());
        
        for (const auto& args : argsContainer) {
            futures.push_back(submit(std::forward<F>(f), args));
        }
        
        return futures;
    }
    
    /**
     * @brief 并行 map 操作
     * @tparam F 可调用类型
     * @tparam Container 参数容器类型
     * @param f 任务函数
     * @param argsContainer 参数容器
     * @return 结果向量
     */
    template<typename F, typename Container>
    auto map(F&& f, const Container& argsContainer)
        -> std::vector<typename std::invoke_result<F, typename Container::value_type>::type> {
        
        auto futures = submitBatch(std::forward<F>(f), argsContainer);
        
        using ReturnType = typename std::invoke_result<F, typename Container::value_type>::type;
        std::vector<ReturnType> results;
        results.reserve(futures.size());
        
        for (auto& future : futures) {
            results.push_back(future.get());
        }
        
        return results;
    }
    
    /**
     * @brief 等待所有任务完成
     */
    void waitAll() {
        std::unique_lock<std::mutex> lock(mutex_);
        doneCondition_.wait(lock, [this] {
            return tasks_.empty() && activeJobs_ == 0;
        });
    }
    
    /**
     * @brief 获取工作线程数
     */
    Size size() const { return workers_.size(); }
    
    /**
     * @brief 获取待处理任务数
     */
    Size pendingTasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }
    
    /**
     * @brief 获取当前活跃任务数
     */
    Size activeJobs() const { return activeJobs_.load(); }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });
                
                if (stop_ && tasks_.empty()) {
                    return;
                }
                
                task = std::move(tasks_.front());
                tasks_.pop();
                ++activeJobs_;
            }
            
            task();
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --activeJobs_;
            }
            doneCondition_.notify_all();
        }
    }
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable doneCondition_;
    
    std::atomic<bool> stop_;
    std::atomic<Size> activeJobs_;
};

/**
 * @brief 全局线程池单例
 */
class GlobalThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }
    
private:
    GlobalThreadPool() = default;
};

/**
 * @brief 并行 for_each
 * @tparam Iterator 迭代器类型
 * @tparam F 可调用类型
 * @param pool 线程池
 * @param begin 开始迭代器
 * @param end 结束迭代器
 * @param f 处理函数
 */
template<typename Iterator, typename F>
void parallelForEach(ThreadPool& pool, Iterator begin, Iterator end, F&& f) {
    std::vector<std::future<void>> futures;
    
    for (auto it = begin; it != end; ++it) {
        futures.push_back(pool.submit([&f, it] { f(*it); }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
}

/**
 * @brief 并行 for
 * @tparam F 可调用类型
 * @param pool 线程池
 * @param start 开始索引
 * @param end 结束索引
 * @param f 处理函数 (接受索引参数)
 */
template<typename F>
void parallelFor(ThreadPool& pool, Size start, Size end, F&& f) {
    std::vector<std::future<void>> futures;
    
    for (Size i = start; i < end; ++i) {
        futures.push_back(pool.submit([&f, i] { f(i); }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
}

/**
 * @brief 分块并行 for (减少任务调度开销)
 * @tparam F 可调用类型
 * @param pool 线程池
 * @param start 开始索引
 * @param end 结束索引
 * @param chunkSize 块大小
 * @param f 处理函数 (接受 start, end 参数)
 */
template<typename F>
void parallelForChunked(ThreadPool& pool, Size start, Size end, Size chunkSize, F&& f) {
    std::vector<std::future<void>> futures;
    
    for (Size i = start; i < end; i += chunkSize) {
        Size chunkEnd = std::min(i + chunkSize, end);
        futures.push_back(pool.submit([&f, i, chunkEnd] { f(i, chunkEnd); }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
}

/**
 * @brief 参数组合生成器
 * 
 * 用于生成策略参数的笛卡尔积
 */
class ParameterGrid {
public:
    using ParamValues = std::vector<ParamValue>;
    using ParamSet = std::map<std::string, ParamValue>;
    
    /**
     * @brief 添加参数范围
     * @param name 参数名
     * @param values 参数值列表
     */
    void addParam(const std::string& name, const ParamValues& values) {
        paramNames_.push_back(name);
        paramValues_.push_back(values);
    }
    
    /**
     * @brief 添加数值参数范围
     * @param name 参数名
     * @param start 起始值
     * @param end 结束值
     * @param step 步长
     */
    void addParam(const std::string& name, Value start, Value end, Value step = 1.0) {
        ParamValues values;
        for (Value v = start; v <= end; v += step) {
            values.push_back(v);
        }
        addParam(name, values);
    }
    
    /**
     * @brief 添加整数参数范围
     * @param name 参数名
     * @param start 起始值
     * @param end 结束值
     * @param step 步长
     */
    void addParamInt(const std::string& name, int start, int end, int step = 1) {
        ParamValues values;
        for (int v = start; v <= end; v += step) {
            values.push_back(v);
        }
        addParam(name, values);
    }
    
    /**
     * @brief 生成所有参数组合
     * @return 参数组合向量
     */
    std::vector<ParamSet> generate() const {
        std::vector<ParamSet> results;
        
        if (paramNames_.empty()) {
            return results;
        }
        
        // 计算总组合数
        Size totalCombinations = 1;
        for (const auto& values : paramValues_) {
            totalCombinations *= values.size();
        }
        
        results.reserve(totalCombinations);
        
        // 生成笛卡尔积
        std::vector<Size> indices(paramNames_.size(), 0);
        
        for (Size i = 0; i < totalCombinations; ++i) {
            ParamSet paramSet;
            for (Size j = 0; j < paramNames_.size(); ++j) {
                paramSet[paramNames_[j]] = paramValues_[j][indices[j]];
            }
            results.push_back(std::move(paramSet));
            
            // 更新索引
            for (Size j = paramNames_.size(); j > 0; --j) {
                Size idx = j - 1;
                ++indices[idx];
                if (indices[idx] < paramValues_[idx].size()) {
                    break;
                }
                indices[idx] = 0;
            }
        }
        
        return results;
    }
    
    /**
     * @brief 获取总组合数
     */
    Size totalCombinations() const {
        Size total = 1;
        for (const auto& values : paramValues_) {
            total *= values.size();
        }
        return total;
    }
    
    /**
     * @brief 清空所有参数
     */
    void clear() {
        paramNames_.clear();
        paramValues_.clear();
    }

private:
    std::vector<std::string> paramNames_;
    std::vector<ParamValues> paramValues_;
};

/**
 * @brief 优化进度回调
 */
struct OptimizationProgress {
    Size completed = 0;        // 已完成数量
    Size total = 0;            // 总数量
    double percentage = 0;     // 完成百分比
    double elapsedSec = 0;     // 已用时间（秒）
    double estimatedSec = 0;   // 预计剩余时间（秒）
};

using OptimizationCallback = std::function<void(const OptimizationProgress&)>;

} // namespace bt
