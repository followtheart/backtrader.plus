/**
 * @file example_sma.cpp
 * @brief SMA 指标使用示例
 */

#include <iostream>
#include <iomanip>
#include "bt/backtrader.hpp"

int main() {
    std::cout << "=== Backtrader C++ SMA Example ===" << std::endl;
    std::cout << "Version: " << bt::version() << std::endl << std::endl;
    
    // 创建测试价格数据
    std::vector<bt::Value> prices = {
        100.0, 101.5, 102.0, 101.0, 103.0,
        104.5, 105.0, 104.0, 106.0, 107.5,
        108.0, 107.0, 109.0, 110.5, 111.0,
        110.0, 112.0, 113.5, 114.0, 113.0
    };
    
    // 创建 LineBuffer 并填充数据
    bt::LineBuffer priceBuffer;
    priceBuffer.extend(prices);
    
    std::cout << "Price data loaded: " << priceBuffer.size() << " bars" << std::endl;
    std::cout << std::endl;
    
    // 创建 SMA 指标
    int smaPeriod = 5;
    bt::SMA sma(&priceBuffer, smaPeriod);
    sma.init();
    
    // 预计算所有值
    sma.precompute();
    
    std::cout << "SMA(" << smaPeriod << ") calculated: " 
              << sma.lines0().size() << " values" << std::endl;
    std::cout << std::endl;
    
    // 打印结果
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Bar\tPrice\tSMA" << std::endl;
    std::cout << "---\t-----\t---" << std::endl;
    
    auto* smaData = sma.lines0().rawData();
    if (smaData) {
        for (size_t i = 0; i < prices.size(); ++i) {
            std::cout << i << "\t" << prices[i] << "\t";
            if (i >= static_cast<size_t>(smaPeriod) - 1 && i - smaPeriod + 1 < smaData->size()) {
                std::cout << (*smaData)[i - smaPeriod + 1];
            } else {
                std::cout << "N/A";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << std::endl;
    
    // 演示 EMA
    std::cout << "=== EMA Example ===" << std::endl;
    bt::EMA ema(&priceBuffer, smaPeriod);
    ema.init();
    ema.precompute();
    
    std::cout << "EMA(" << smaPeriod << ") calculated: "
              << ema.lines0().size() << " values" << std::endl;
    
    // 演示 RSI
    std::cout << std::endl;
    std::cout << "=== RSI Example ===" << std::endl;
    bt::RSI rsi(&priceBuffer, 5);
    rsi.init();
    rsi.precompute();
    
    std::cout << "RSI(5) calculated: " << rsi.lines0().size() << " values" << std::endl;
    
    // 演示 BollingerBands
    std::cout << std::endl;
    std::cout << "=== Bollinger Bands Example ===" << std::endl;
    bt::BollingerBands bb(&priceBuffer, 5, 2.0);
    bb.init();
    bb.precompute();
    
    std::cout << "Bollinger(5, 2.0) calculated:" << std::endl;
    std::cout << "  - Mid band: " << bb.mid().size() << " values" << std::endl;
    std::cout << "  - Top band: " << bb.top().size() << " values" << std::endl;
    std::cout << "  - Bot band: " << bb.bot().size() << " values" << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== Done ===" << std::endl;
    
    return 0;
}
