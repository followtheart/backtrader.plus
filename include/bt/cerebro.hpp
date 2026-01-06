/**
 * @file cerebro.hpp
 * @brief Cerebro - Main orchestration engine
 * 
 * Corresponds to Python's cerebro.py
 * The Cerebro class is the central hub that coordinates:
 * - Data feeds
 * - Strategies
 * - Broker
 * - Analyzers
 * - Observers
 */

#pragma once

#include "bt/params.hpp"
#include "bt/strategy.hpp"
#include "bt/broker.hpp"
#include "bt/datafeed.hpp"
#include "bt/analyzer.hpp"
#include "bt/observer.hpp"
#include "bt/sizer.hpp"
#include "bt/writer.hpp"
#include "bt/filter.hpp"
#include "bt/timer.hpp"
#include "bt/threadpool.hpp"
#include "bt/optimizer.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <limits>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace bt {

/**
 * @brief Run result containing backtesting statistics
 */
struct RunResult {
    Value startCash = 0;
    Value endCash = 0;
    Value endValue = 0;
    Value pnl = 0;
    Value pnlPct = 0;
    Size totalBars = 0;
    Size totalTrades = 0;
    std::vector<Trade> trades;
    std::map<std::string, Value> analysis;
    std::vector<Strategy*> strategies;
};

/**
 * @brief Optimize result for parameter optimization
 */
struct OptimizeResult {
    std::map<std::string, ParamValue> params;
    RunResult result;
};

/**
 * @brief Cerebro - Main backtesting engine
 * 
 * This is the main entry point for running backtests.
 * It orchestrates the interaction between data feeds, strategies,
 * the broker, and various analyzers/observers.
 * 
 * Usage:
 * @code
 * bt::Cerebro cerebro;
 * 
 * // Add data
 * auto data = std::make_shared<bt::BacktraderCSVData>("data.csv");
 * data->load();
 * cerebro.addData(data);
 * 
 * // Add strategy
 * cerebro.addStrategy<MyStrategy>();
 * 
 * // Configure broker
 * cerebro.broker().setCash(100000);
 * 
 * // Run backtest
 * auto results = cerebro.run();
 * @endcode
 */
class Cerebro : public Parametrized<Cerebro> {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(preload, true)      // Preload data before running
        BT_PARAM(runonce, true)      // Vectorized indicator calculation
        BT_PARAM(live, false)        // Live trading mode
        BT_PARAM(stdstats, true)     // Add standard observers
        BT_PARAM(exactbars, false)   // Memory saving mode
        BT_PARAM(maxcpus, 0)         // Max CPUs for optimization (0 = auto)
        BT_PARAM(optdatas, true)     // Share data in optimization
        BT_PARAM(optreturn, true)    // Return minimal optimization results
        BT_PARAM(cheat_on_open, false) // Enable cheat-on-open mode
        BT_PARAM(cheat_on_close, false) // Enable cheat-on-close mode
        BT_PARAM(broker_coo, true)   // Broker processes COO orders
        BT_PARAM(quicknotify, false) // Quick notification mode
    BT_PARAMS_END()

    Cerebro() : broker_(std::make_unique<Broker>()) {}
    ~Cerebro() = default;
    
    // Non-copyable
    Cerebro(const Cerebro&) = delete;
    Cerebro& operator=(const Cerebro&) = delete;
    
    // Move-constructible
    Cerebro(Cerebro&&) = default;
    Cerebro& operator=(Cerebro&&) = default;

    // ==================== Data Management ====================
    
    /**
     * @brief Add a data feed
     * @param data Shared pointer to data feed
     * @param name Optional name for the data feed
     */
    void addData(std::shared_ptr<DataFeed> data, const std::string& name = "") {
        std::string n = name.empty() ? "data" + std::to_string(datas_.size()) : name;
        data->setName(n);
        datas_.push_back(data);
        broker_->addDataFeed(n, data.get());
    }
    
    /**
     * @brief Get data feed by index
     */
    DataFeed* getData(Size idx = 0) const {
        return idx < datas_.size() ? datas_[idx].get() : nullptr;
    }
    
    /**
     * @brief Get number of data feeds
     */
    Size dataCount() const { return datas_.size(); }

    // ==================== Strategy Management ====================
    
    /**
     * @brief Add a strategy class to be instantiated at run time
     * @tparam T Strategy class type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     */
    template<typename T, typename... Args>
    void addStrategy(Args&&... args) {
        // Capture args by value to avoid dangling references
        strategyFactories_.push_back([args...]() {
            return std::make_unique<T>(args...);
        });
    }

    // ==================== Broker Access ====================
    
    /**
     * @brief Get reference to the broker
     */
    Broker& broker() { return *broker_; }
    const Broker& broker() const { return *broker_; }
    
    /**
     * @brief Set broker cash
     */
    void setCash(Value cash) { broker_->setCash(cash); }

    // ==================== Analyzers ====================
    
    /**
     * @brief Add an analyzer
     * @tparam T Analyzer class type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the created analyzer
     */
    template<typename T, typename... Args>
    T* addAnalyzer(Args&&... args) {
        auto analyzer = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = analyzer.get();
        analyzers_.push_back(std::move(analyzer));
        return ptr;
    }
    
    // ==================== Observers ====================
    
    /**
     * @brief Add an observer
     * @tparam T Observer class type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the created observer
     */
    template<typename T, typename... Args>
    T* addObserver(Args&&... args) {
        auto observer = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = observer.get();
        observers_.push_back(std::move(observer));
        return ptr;
    }
    
    // ==================== Sizers ====================
    
    /**
     * @brief Add a sizer for all strategies
     * @tparam T Sizer class type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     */
    template<typename T, typename... Args>
    void addSizer(Args&&... args) {
        sizerFactory_ = [args...]() {
            return std::make_unique<T>(args...);
        };
    }
    
    /**
     * @brief Add a sizer by data index
     * @param idx Data feed index
     * @tparam T Sizer class type
     * @tparam Args Constructor argument types
     */
    template<typename T, typename... Args>
    void addSizerByIdx(Size idx, Args&&... args) {
        sizerFactoriesByIdx_[idx] = [args...]() {
            return std::make_unique<T>(args...);
        };
    }
    
    // ==================== Writers ====================
    
    /**
     * @brief Add a writer
     * @tparam T Writer class type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the created writer
     */
    template<typename T, typename... Args>
    T* addWriter(Args&&... args) {
        auto writer = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = writer.get();
        writers_.push_back(std::move(writer));
        return ptr;
    }
    
    // ==================== Filters ====================
    
    /**
     * @brief Add a filter to a data feed
     * @param dataIdx Index of data feed to filter
     * @tparam T Filter class type
     * @tparam Args Constructor argument types
     */
    template<typename T, typename... Args>
    void addFilter(Size dataIdx, Args&&... args) {
        if (dataIdx < filters_.size()) {
            filters_[dataIdx].push_back(std::make_unique<T>(std::forward<Args>(args)...));
        }
    }
    
    // ==================== Timers ====================
    
    /**
     * @brief Add a global timer
     * @param when Time of day to trigger
     * @param offsetMinutes Offset in minutes
     * @param repeatMinutes Repeat interval
     * @return Timer ID
     */
    int addTimer(const TimeOfDay& when, int offsetMinutes = 0, int repeatMinutes = 0) {
        return timerManager_.addTimer(when, offsetMinutes, repeatMinutes);
    }
    
    /**
     * @brief Add a timer with full configuration
     */
    int addTimer(Timer timer) {
        return timerManager_.addTimer(std::move(timer));
    }
    
    /**
     * @brief Get timer manager
     */
    TimerManager& timerManager() { return timerManager_; }

    // ==================== Running ====================
    
    /**
     * @brief Run the backtest
     * @return Vector of run results (one per strategy)
     */
    std::vector<RunResult> run();
    
    /**
     * @brief Stop the running backtest
     */
    void stop() { stopRequested_ = true; }
    
    /**
     * @brief Check if stop was requested
     */
    bool stopRequested() const { return stopRequested_; }
    
    // ==================== Optimization ====================
    
    /**
     * @brief Add a strategy for optimization with parameter ranges
     * 
     * Similar to Python's cerebro.optstrategy()
     * 
     * @tparam T Strategy class type
     * @param grid Parameter grid for optimization
     */
    template<typename T>
    void optStrategy(const ParameterGrid& grid) {
        doOptimize_ = true;
        optGrid_ = grid;
        optStrategyFactory_ = []() {
            return std::make_unique<T>();
        };
    }
    
    /**
     * @brief Add optimization callback
     * 
     * Called after each strategy run during optimization
     */
    void optCallback(std::function<void(const OptResult&)> cb) {
        optCallbacks_.push_back(std::move(cb));
    }
    
    /**
     * @brief Run optimization
     * @return Vector of optimization results
     */
    std::vector<OptResult> runOptimize();
    
    /**
     * @brief Check if in optimization mode
     */
    bool isOptimizing() const { return doOptimize_; }

private:
    // Setup methods
    void setupBroker();
    void setupStrategies();
    void setupAnalyzers();
    void setupObservers();
    void preloadData();
    void resetState();
    
    // Run methods
    RunResult runOnce(Strategy* strategy);
    RunResult runNext(Strategy* strategy);
    void brokerNotify();
    
    // Components
    std::unique_ptr<Broker> broker_;
    std::vector<std::shared_ptr<DataFeed>> datas_;
    std::vector<std::function<std::unique_ptr<Strategy>()>> strategyFactories_;
    std::vector<std::unique_ptr<Strategy>> strategyInstances_;
    std::vector<std::unique_ptr<Analyzer>> analyzers_;
    std::vector<std::unique_ptr<Observer>> observers_;
    std::vector<std::unique_ptr<Writer>> writers_;
    std::vector<std::vector<std::unique_ptr<DataFilter>>> filters_;
    TimerManager timerManager_;
    
    // Sizer factories
    std::function<std::unique_ptr<Sizer>()> sizerFactory_;
    std::unordered_map<Size, std::function<std::unique_ptr<Sizer>()>> sizerFactoriesByIdx_;
    
    // State
    bool stopRequested_ = false;
    
    // Optimization state
    bool doOptimize_ = false;
    ParameterGrid optGrid_;
    std::function<std::unique_ptr<Strategy>()> optStrategyFactory_;
    std::vector<std::function<void(const OptResult&)>> optCallbacks_;
    std::mutex optMutex_;
};

} // namespace bt
