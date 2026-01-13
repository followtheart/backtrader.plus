/**
 * @file benchmark_indicators.cpp
 * @brief 指标性能基准测试
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <random>
#include "bt/backtrader.hpp"

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;

// 生成随机价格数据
std::vector<bt::Value> generateRandomPrices(size_t count, bt::Value startPrice = 100.0) {
    std::vector<bt::Value> prices;
    prices.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0.0, 1.0);
    
    bt::Value price = startPrice;
    for (size_t i = 0; i < count; ++i) {
        price += dis(gen);
        prices.push_back(price);
    }
    
    return prices;
}

template<typename Indicator, typename... Args>
Duration benchmarkIndicator(const std::string& name, bt::LineBuffer& data, 
                           int iterations, Args&&... args) {
    Duration total(0);
    
    for (int i = 0; i < iterations; ++i) {
        // 1. 重置输入数据 Cursor !!! 重要 !!!
        // 否则后续计算都会基于最后一个数据点重复计算，导致缓存虚假命中
        data.home();

        auto start = Clock::now();
        
        Indicator ind(&data, std::forward<Args>(args)...);
        ind.init();
        
        // 2. 手动驱动回测循环 (模拟 Cerebro)
        // Indicator::precompute() 默认实现不驱动 input data，仅适用于向量化模式
        // 这里我们模拟 Event-driven 模式以获得更真实的性能数据
        
        bt::Size len = data.length();
        // 如果数据长度小于最小周期，指标无法计算
        bt::Size min_p = ind.minperiod();
        
        // 我们通常从 0 开始驱动，或者从 min_period 开始
        // 这里简单地遍历整个数据
        for (bt::Size j = 0; j < len; ++j) {
            // 输入数据必须与指标同步步进
            // 注意：LineBuffer::home() 将 pos 置为 0
            // 在循环末尾我们 advance()
            
            // 只有当数据足够时才计算 (Backtrader 逻辑)
            // 但为了 benchmark 纯计算压力，我们全程调用 next
            if (j >= min_p - 1) {
                ind.next();
            }
            
            // 驱动指标输出游标 (如果是手动调用 next，通常 ind 内部 push 会自动处理，
            // 但 Indicator::advance 是为了那些没有使用 push 而是直接写内存的场景
            // 标准指标使用 push，不需要手动 advance 输出? 
            // 不，Indicator 继承 LineSeries，需要管理多条线。
            // 大多数 Indicator 实现如 SMA, Bollinger 使用 lines0().push()
            // push() 会自动增加内部 storage 的 pos。
            // 但是 Indicator 作为一个 wrapper，我们需要确保它所有状态一致。
            // 实际上，只要 next() 内部做了 push，我们就不用管。
            
            // 驱动输入数据
            data.advance();
        }
        
        auto end = Clock::now();
        total += std::chrono::duration_cast<Duration>(end - start);
    }
    
    return total / iterations;
}

int main() {
    std::cout << "=== Backtrader C++ Performance Benchmark ===" << std::endl;
    std::cout << "Version: " << bt::version() << std::endl << std::endl;
    
    // 测试不同数据规模
    std::vector<size_t> dataSizes = {1000, 10000, 100000, 1000000};
    int iterations = 10;
    
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Data Size\tSMA(20)\t\tEMA(20)\t\tRSI(14)\t\tBollinger(20)" << std::endl;
    std::cout << "---------\t-------\t\t-------\t\t-------\t\t-------------" << std::endl;
    
    for (size_t size : dataSizes) {
        std::cout << size << "\t\t";
        
        // 生成数据
        auto prices = generateRandomPrices(size);
        bt::LineBuffer data;
        data.extend(prices);
        
        // SMA 基准
        auto smaDuration = benchmarkIndicator<bt::SMA>("SMA", data, iterations, 20);
        std::cout << smaDuration.count() << " ms\t\t";
        
        // EMA 基准
        auto emaDuration = benchmarkIndicator<bt::EMA>("EMA", data, iterations, 20);
        std::cout << emaDuration.count() << " ms\t\t";
        
        // RSI 基准
        auto rsiDuration = benchmarkIndicator<bt::RSI>("RSI", data, iterations, 14);
        std::cout << rsiDuration.count() << " ms\t\t";
        
        // Bollinger 基准
        auto bbDuration = benchmarkIndicator<bt::BollingerBands>("BB", data, iterations, 20, 2.0);
        std::cout << bbDuration.count() << " ms";
        
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    
    // 详细的单次测试（100万数据点）
    std::cout << "=== Detailed Benchmark (1M data points) ===" << std::endl;
    
    auto prices = generateRandomPrices(1000000);
    bt::LineBuffer largeData;
    largeData.extend(prices);
    
    {
        // 确保数据指针重置
        largeData.home();
        
        auto start = Clock::now();
        bt::SMA sma(&largeData, 200);
        sma.init();
        
        // 手动驱动
        bt::Size len = largeData.length();
        bt::Size min_p = sma.minperiod();
        
        for (bt::Size j = 0; j < len; ++j) {
            if (j >= min_p - 1) {
                sma.next();
            }
            largeData.advance(); // 步进数据
        }
        
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end - start);
        
        std::cout << "SMA(200) on 1M points: " << duration.count() << " ms" << std::endl;
        std::cout << "  Output size: " << sma.lines0().size() << " values" << std::endl;
        std::cout << "  Throughput: " << (1000000.0 / duration.count() * 1000.0) 
                  << " values/sec" << std::endl;
    }
    
    std::cout << std::endl;
    
    // 内存测试
    std::cout << "=== Memory Usage Estimate ===" << std::endl;
    
    // LineBuffer 内存
    size_t lineBufferSize = sizeof(bt::LineBuffer) + 1000000 * sizeof(bt::Value);
    std::cout << "LineBuffer (1M values): ~" << (lineBufferSize / 1024.0 / 1024.0) 
              << " MB" << std::endl;
    
    // QBuffer 内存
    bt::LineBuffer qbuf(1000);  // 只保留 1000 个值
    qbuf.extend(prices);
    size_t qbufSize = sizeof(bt::LineBuffer) + 1000 * sizeof(bt::Value);
    std::cout << "QBuffer (1K max): ~" << (qbufSize / 1024.0) << " KB" << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== Benchmark Complete ===" << std::endl;
    
    return 0;
}
