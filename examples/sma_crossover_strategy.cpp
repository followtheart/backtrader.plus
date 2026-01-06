/**
 * @file sma_crossover_strategy.cpp
 * @brief SMA Crossover Strategy - Complete backtest example
 * 
 * Demonstrates a full backtesting workflow with Cerebro.
 */

#include "bt/backtrader.hpp"
#include <iostream>
#include <iomanip>

/**
 * @brief Simple SMA Crossover Strategy
 * 
 * Buy when fast SMA crosses above slow SMA.
 * Sell when fast SMA crosses below slow SMA.
 */
class SMACrossover : public bt::Strategy {
public:
    SMACrossover(bt::Size fastPeriod = 10, bt::Size slowPeriod = 30)
        : fastPeriod_(fastPeriod), slowPeriod_(slowPeriod) {}
    
    void init() override {
        setMinPeriod(slowPeriod_);
    }
    
    void next() override {
        if (!data(0)) return;
        
        auto& closePrice = data(0)->close();
        if (closePrice.length() < slowPeriod_) return;
        
        // Calculate fast and slow SMA
        double fastSMA = calculateSMA(closePrice, fastPeriod_);
        double slowSMA = calculateSMA(closePrice, slowPeriod_);
        
        // Calculate previous SMAs for crossover detection
        double prevFastSMA = calculateSMA(closePrice, fastPeriod_, 1);
        double prevSlowSMA = calculateSMA(closePrice, slowPeriod_, 1);
        
        // Check for crossover
        bool crossAbove = prevFastSMA <= prevSlowSMA && fastSMA > slowSMA;
        bool crossBelow = prevFastSMA >= prevSlowSMA && fastSMA < slowSMA;
        
        bt::Value pos = position();
        
        if (crossAbove && pos <= 0) {
            if (pos < 0) closePosition();  // Close short
            buy();
            std::cout << "BUY  @ " << std::fixed << std::setprecision(2) 
                      << closePrice[0] << " | Fast SMA: " << fastSMA 
                      << " | Slow SMA: " << slowSMA << std::endl;
        }
        else if (crossBelow && pos >= 0) {
            if (pos > 0) closePosition();  // Close long
            sell();
            std::cout << "SELL @ " << std::fixed << std::setprecision(2) 
                      << closePrice[0] << " | Fast SMA: " << fastSMA 
                      << " | Slow SMA: " << slowSMA << std::endl;
        }
    }
    
    void notifyTrade(bt::Trade& trade) override {
        if (!trade.isOpen) {
            std::cout << "TRADE CLOSED | PnL: " << std::fixed << std::setprecision(2) 
                      << trade.pnlComm << std::endl;
        }
    }

private:
    bt::Size fastPeriod_;
    bt::Size slowPeriod_;
    
    double calculateSMA(const bt::LineBuffer& data, bt::Size period, bt::Size offset = 0) {
        if (data.length() < period + offset) return 0;
        
        double sum = 0;
        for (bt::Size i = offset; i < period + offset; ++i) {
            sum += data[i];
        }
        return sum / period;
    }
};

int main() {
    std::cout << "=== SMA Crossover Strategy Backtest ===" << std::endl;
    std::cout << "Backtrader C++ Version: " << bt::version() << std::endl;
    std::cout << std::endl;
    
    // Create Cerebro
    bt::Cerebro cerebro;
    
    // Create sample data (simulated price data)
    auto data = std::make_shared<bt::MemoryDataFeed>();
    
    // Generate 100 days of price data with a trend
    double price = 100.0;
    for (int i = 0; i < 100; ++i) {
        bt::DateTime dt(2024, 1, i / 30 + 1, i % 30 + 1);
        
        // Add some randomness and trend
        double change = (i < 40) ? 0.5 : (i < 70) ? -0.3 : 0.4;
        double noise = ((i * 17) % 10 - 5) * 0.1;
        price = price + change + noise;
        
        data->addBar(dt, price - 1, price + 1, price - 2, price, 10000, 0);
    }
    
    // Add data to cerebro
    cerebro.addData(data, "SAMPLE");
    
    // Set broker parameters
    cerebro.setCash(100000.0);
    cerebro.broker().setCommission(std::make_shared<bt::CommInfoStock>(0.001));
    
    // Add strategy
    cerebro.addStrategy<SMACrossover>(10, 30);
    
    // Add analyzers
    auto* sharpe = cerebro.addAnalyzer<bt::SharpeRatio>();
    auto* drawdown = cerebro.addAnalyzer<bt::DrawDown>();
    
    std::cout << "Starting Cash: $" << std::fixed << std::setprecision(2) 
              << cerebro.broker().getCash() << std::endl;
    std::cout << std::endl;
    
    // Run backtest
    std::cout << "--- Running Backtest ---" << std::endl;
    auto results = cerebro.run();
    std::cout << "--- Backtest Complete ---" << std::endl;
    std::cout << std::endl;
    
    // Print results
    if (!results.empty()) {
        auto& result = results[0];
        
        std::cout << "=== Results ===" << std::endl;
        std::cout << "Final Portfolio Value: $" << std::fixed << std::setprecision(2) 
                  << result.endValue << std::endl;
        std::cout << "Total Return: " << result.pnlPct << "%" << std::endl;
        std::cout << "Total Trades: " << result.totalTrades << std::endl;
        
        // Analyzer results
        auto sharpeAnalysis = sharpe->getAnalysis();
        if (sharpeAnalysis.count("sharpe_ratio")) {
            std::cout << "Sharpe Ratio: " << sharpeAnalysis["sharpe_ratio"] << std::endl;
        }
        
        auto ddAnalysis = drawdown->getAnalysis();
        if (ddAnalysis.count("max_drawdown")) {
            std::cout << "Max Drawdown: " << ddAnalysis["max_drawdown"] << "%" << std::endl;
        }
    }
    
    return 0;
}
