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
        auto start = Clock::now();
        
        Indicator ind(&data, std::forward<Args>(args)...);
        ind.init();
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
        auto start = Clock::now();
        bt::SMA sma(&largeData, 200);
        sma.init();
        sma.precompute();
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
