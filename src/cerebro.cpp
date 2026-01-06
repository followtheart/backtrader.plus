/**
 * @file cerebro.cpp
 * @brief Cerebro implementation - Main backtesting engine
 * 
 * Corresponds to Python's cerebro.py
 */

#include "bt/cerebro.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace bt {

std::vector<RunResult> Cerebro::run() {
    std::vector<RunResult> results;
    
    // Check if we have data
    if (datas_.empty()) {
        return results;  // Nothing to run
    }
    
    // Reset stop flag
    stopRequested_ = false;
    
    // Determine run mode based on params
    bool doPreload = p().get<bool>("preload");
    bool doRunonce = p().get<bool>("runonce");
    bool isLive = p().get<bool>("live");
    
    // Live mode disables preload and runonce
    if (isLive) {
        doPreload = false;
        doRunonce = false;
    }
    
    // Preload data if requested
    if (doPreload) {
        preloadData();
    }
    
    // Setup broker
    setupBroker();
    
    // Create strategy instances from factories
    strategyInstances_.clear();
    for (auto& factory : strategyFactories_) {
        strategyInstances_.push_back(factory());
    }
    
    // If no strategies, add a default empty one
    if (strategyInstances_.empty()) {
        strategyInstances_.push_back(std::make_unique<Strategy>());
    }
    
    // Setup strategies
    setupStrategies();
    
    // Setup analyzers and observers
    setupAnalyzers();
    setupObservers();
    
    // Run each strategy
    for (auto& strategy : strategyInstances_) {
        RunResult result;
        
        if (doRunonce && doPreload) {
            result = runOnce(strategy.get());
        } else {
            result = runNext(strategy.get());
        }
        
        results.push_back(result);
        
        if (stopRequested_) break;
    }
    
    return results;
}

void Cerebro::setupBroker() {
    // Reset broker to initial state
    broker_->reset();
}

void Cerebro::setupStrategies() {
    for (auto& strategy : strategyInstances_) {
        strategy->setCerebro(this);
        strategy->setBroker(broker_.get());
        
        // Add data feeds to strategy
        for (Size i = 0; i < datas_.size(); ++i) {
            strategy->addData(datas_[i].get(), datas_[i]->name());
        }
        
        // Call init to create indicators
        strategy->init();
    }
}

void Cerebro::setupAnalyzers() {
    for (auto& analyzer : analyzers_) {
        if (!strategyInstances_.empty()) {
            analyzer->setStrategy(strategyInstances_[0].get());
        }
        analyzer->setBroker(broker_.get());
    }
}

void Cerebro::setupObservers() {
    // Add standard observers if requested
    if (p().get<bool>("stdstats")) {
        // Cash observer
        auto cash = std::make_unique<CashObserver>();
        cash->setBroker(broker_.get());
        observers_.push_back(std::move(cash));
        
        // Value observer
        auto value = std::make_unique<ValueObserver>();
        value->setBroker(broker_.get());
        observers_.push_back(std::move(value));
    }
    
    // Setup other observers
    for (auto& observer : observers_) {
        if (!strategyInstances_.empty()) {
            observer->setStrategy(strategyInstances_[0].get());
        }
        observer->setBroker(broker_.get());
    }
}

void Cerebro::preloadData() {
    // Load all data feeds
    for (auto& data : datas_) {
        if (!data->length()) {
            data->load();
        }
    }
}

void Cerebro::resetState() {
    // Reset data feeds to beginning
    for (auto& data : datas_) {
        data->reset();
    }
}

RunResult Cerebro::runOnce(Strategy* strategy) {
    RunResult result;
    result.startCash = broker_->getCash();
    result.strategies.push_back(strategy);
    
    // Find minimum data length
    Size minLen = std::numeric_limits<Size>::max();
    for (auto& data : datas_) {
        if (data->length() > 0) {
            minLen = std::min(minLen, data->length());
        }
    }
    
    if (minLen == 0 || minLen == std::numeric_limits<Size>::max()) {
        result.endCash = result.startCash;
        result.endValue = result.startCash;
        return result;
    }
    
    // Get minimum period from strategy
    Size minPeriod = strategy->minPeriod();
    if (minPeriod == 0) minPeriod = 1;
    
    // Start analyzers and observers
    for (auto& analyzer : analyzers_) {
        analyzer->start();
    }
    for (auto& observer : observers_) {
        observer->start();
    }
    
    // Call strategy start
    strategy->start();
    
    // Track first call after warmup
    bool calledNextStart = false;
    
    // Main loop
    for (Size bar = 0; bar < minLen; ++bar) {
        if (stopRequested_) break;
        
        // Position data at current bar
        for (auto& data : datas_) {
            data->reset();
            for (Size i = 0; i <= bar; ++i) {
                data->next();
            }
        }
        
        // Update bar info
        strategy->setBarIndex(bar);
        strategy->setBarLength(minLen);
        
        // Process broker (execute pending orders)
        broker_->next();
        brokerNotify();
        
        // Call appropriate lifecycle method
        if (bar < minPeriod - 1) {
            strategy->prenext();
        } else if (!calledNextStart) {
            strategy->nextstart();
            calledNextStart = true;
        } else {
            strategy->next();
        }
        
        // Notify cash/value
        strategy->notifyCashValue(broker_->getCash(), broker_->getValue());
        
        // Update analyzers and observers
        for (auto& analyzer : analyzers_) {
            analyzer->next();
        }
        for (auto& observer : observers_) {
            observer->next();
        }
    }
    
    // Stop
    strategy->stop();
    
    for (auto& analyzer : analyzers_) {
        analyzer->stop();
        // Collect analysis
        auto analysis = analyzer->getAnalysis();
        for (auto& [key, val] : analysis) {
            result.analysis[key] = val;
        }
    }
    for (auto& observer : observers_) {
        observer->stop();
    }
    
    // Compile results
    result.endCash = broker_->getCash();
    result.endValue = broker_->getValue();
    result.pnl = result.endValue - result.startCash;
    result.pnlPct = result.startCash > 0 ? (result.pnl / result.startCash * 100.0) : 0;
    result.totalBars = minLen;
    result.trades = broker_->getTrades();
    result.totalTrades = result.trades.size();
    
    return result;
}

RunResult Cerebro::runNext(Strategy* strategy) {
    // Same as runOnce for now, but designed for event-driven mode
    // In future, this would handle live data differently
    return runOnce(strategy);
}

void Cerebro::brokerNotify() {
    // In future, this would handle broker notifications
    // For now, broker callbacks handle this directly
}

std::vector<OptResult> Cerebro::runOptimize() {
    std::vector<OptResult> results;
    
    if (!doOptimize_ || !optStrategyFactory_) {
        return results;
    }
    
    // Generate all parameter combinations
    auto paramSets = optGrid_.generate();
    if (paramSets.empty()) {
        return results;
    }
    
    // Determine thread count
    int maxCpus = p().get<int>("maxcpus");
    Size numThreads = (maxCpus > 0) ? static_cast<Size>(maxCpus) : 
                      static_cast<Size>(std::thread::hardware_concurrency());
    if (numThreads == 0) numThreads = 1;
    
    // Create thread pool
    ThreadPool pool(numThreads);
    
    // Track progress
    std::atomic<Size> completed{0};
    Size total = paramSets.size();
    
    // Preload data once if optdatas is enabled
    if (p().get<bool>("optdatas") && p().get<bool>("preload")) {
        preloadData();
    }
    
    // Submit optimization tasks
    std::vector<std::future<OptResult>> futures;
    futures.reserve(total);
    
    for (const auto& params : paramSets) {
        futures.push_back(pool.submit([this, params, &completed, total]() -> OptResult {
            OptResult result;
            result.params = params;
            
            try {
                // Create a fresh Cerebro for this run
                Cerebro localCerebro;
                
                // Copy configuration
                localCerebro.p().set("preload", p().get<bool>("preload"));
                localCerebro.p().set("runonce", p().get<bool>("runonce"));
                localCerebro.p().set("stdstats", p().get<bool>("stdstats"));
                localCerebro.broker().setCash(broker_->getCash());
                // Note: Commission copying would require additional API
                
                // Share data feeds (they are read-only)
                for (auto& data : datas_) {
                    localCerebro.addData(data, data->name());
                }
                
                // Create and configure strategy with params using the captured factory
                auto stratFactory = optStrategyFactory_;
                auto paramsCapture = params;
                localCerebro.strategyFactories_.push_back([stratFactory, paramsCapture]() {
                    auto strategy = stratFactory();
                    for (const auto& [name, value] : paramsCapture) {
                        strategy->p().set(name, value);
                    }
                    return strategy;
                });
                
                // Run backtest
                auto runResults = localCerebro.run();
                
                if (!runResults.empty()) {
                    const auto& runResult = runResults[0];
                    result.finalValue = runResult.endValue;
                    result.pnl = runResult.pnl;
                    result.pnlPct = runResult.pnlPct;
                    result.totalTrades = runResult.totalTrades;
                    
                    // Extract additional metrics from analysis
                    auto it = runResult.analysis.find("sharpe_ratio");
                    if (it != runResult.analysis.end()) {
                        result.sharpeRatio = it->second;
                    }
                    
                    it = runResult.analysis.find("max_drawdown");
                    if (it != runResult.analysis.end()) {
                        result.maxDrawdown = it->second;
                    }
                    
                    // Calculate win rate from trades
                    Size winCount = 0;
                    for (const auto& trade : runResult.trades) {
                        if (trade.pnl > 0) ++winCount;
                    }
                    if (result.totalTrades > 0) {
                        result.winningTrades = winCount;
                        result.winRate = static_cast<Value>(winCount) / 
                                        static_cast<Value>(result.totalTrades) * 100.0;
                    }
                }
            } catch (const std::exception& e) {
                // Log error but continue optimization
                result.pnl = -1e10;
                result.pnlPct = -1e10;
            }
            
            ++completed;
            return result;
        }));
    }
    
    // Collect results
    results.reserve(total);
    for (auto& future : futures) {
        auto result = future.get();
        
        {
            std::lock_guard<std::mutex> lock(optMutex_);
            results.push_back(result);
            
            // Call optimization callbacks
            for (auto& cb : optCallbacks_) {
                cb(result);
            }
        }
    }
    
    // Sort by PnL% descending
    std::sort(results.begin(), results.end(), std::greater<OptResult>());
    
    return results;
}

} // namespace bt
