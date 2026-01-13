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
        // 1. 重置输入数据 Cursor
        data.home();

        auto start = Clock::now();
        
        Indicator ind(&data, std::forward<Args>(args)...);
        ind.init();
        
        // 2. 手动驱动回测循环 (模拟 Cerebro Event-driven 模式)
        bt::Size len = data.length();
        bt::Size min_p = ind.minperiod();
        
        for (bt::Size j = 0; j < len; ++j) {
            // 只有当数据足够时才计算
            if (j >= min_p - 1) {
                ind.next();
            }
            // 驱动输入数据
            data.advance();
        }
        
        auto end = Clock::now();
        total += std::chrono::duration_cast<Duration>(end - start);
    }
    
    return total / iterations;
}

template<typename Indicator, typename... Args>
Duration benchmarkIndicatorVectorized(const std::string& name, bt::LineBuffer& data, 
                           int iterations, Args&&... args) {
    Duration total(0);
    
    for (int i = 0; i < iterations; ++i) {
        // 向量化计算通常不需要外部手动驱动每一个 tick
        // 但为了公平对比，我们同样需要确保输入数据处于可访问状态
        // 不过 once/runonce 模式通常会直接操作底层数组或批量处理
        
        // Reset 虽非必须（如果实现是 pure function），但为了保险
        data.home();
        
        auto start = Clock::now();
        
        Indicator ind(&data, std::forward<Args>(args)...);
        ind.init();
        
        // 核心差异：一次性调用 precompute/once
        // 这将触发 SIMD 优化路径（如果已实现）
        // precompute 内部会自动寻找数据长度并调用 once(0, len)
        ind.precompute();
        
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
    std::cout << "Data Size\tMode\t\tSMA(20)\t\tEMA(20)\t\tRSI(14)\t\tBollinger(20)" << std::endl;
    std::cout << "---------\t----\t\t-------\t\t-------\t\t-------\t\t-------------" << std::endl;
    
    for (size_t size : dataSizes) {
        // 生成数据
        auto prices = generateRandomPrices(size);
        bt::LineBuffer data;
        data.extend(prices);
        
        // --- 1. Event Driven (Scalar/Next) ---
        std::cout << size << "\t\tNext\t\t";
        
        auto smaNext = benchmarkIndicator<bt::SMA>("SMA", data, iterations, 20);
        std::cout << smaNext.count() << " ms\t\t";
        
        auto emaNext = benchmarkIndicator<bt::EMA>("EMA", data, iterations, 20);
        std::cout << emaNext.count() << " ms\t\t";
        
        auto rsiNext = benchmarkIndicator<bt::RSI>("RSI", data, iterations, 14);
        std::cout << rsiNext.count() << " ms\t\t";
        
        auto bbNext = benchmarkIndicator<bt::BollingerBands>("BB", data, iterations, 20, 2.0);
        std::cout << bbNext.count() << " ms" << std::endl;

        // --- 2. Vectorized (SIMD/Once) ---
        // 只有当规模较大时，向量化的优势才明显，且小于一定规模时也没必要分开展示，
        // 但为了对比清晰，我们全部展示
        std::cout << size << "\t\tVector\t\t";
        
        auto smaVec = benchmarkIndicatorVectorized<bt::SMA>("SMA", data, iterations, 20);
        std::cout << smaVec.count() << " ms\t\t";
        
        auto emaVec = benchmarkIndicatorVectorized<bt::EMA>("EMA", data, iterations, 20);
        std::cout << emaVec.count() << " ms\t\t";
        
        auto rsiVec = benchmarkIndicatorVectorized<bt::RSI>("RSI", data, iterations, 14);
        std::cout << rsiVec.count() << " ms\t\t";
        
        auto bbVec = benchmarkIndicatorVectorized<bt::BollingerBands>("BB", data, iterations, 20, 2.0);
        std::cout << bbVec.count() << " ms" << std::endl;
        
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
