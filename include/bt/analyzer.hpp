/**
 * @file analyzer.hpp
 * @brief Analyzer System
 * 
 * Corresponds to Python's analyzer.py and analyzers/
 * Analyzers compute statistics about strategy performance.
 */

#pragma once

#include "bt/params.hpp"
#include "bt/order.hpp"
#include <vector>
#include <map>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace bt {

class Strategy;
class Broker;
class DataFeed;

/**
 * @brief Base Analyzer class
 * 
 * Analyzers operate in the frame of a strategy and provide
 * analysis/statistics for that strategy's performance.
 * 
 * Lifecycle mirrors Strategy:
 * - start() - Begin of operations
 * - prenext() - While warming up
 * - nextstart() - First bar with valid data
 * - next() - Each subsequent bar
 * - stop() - End of operations
 */
class Analyzer : public Parametrized<Analyzer> {
public:
    virtual ~Analyzer() = default;
    
    // Lifecycle methods
    virtual void start() {}
    virtual void prenext() {}
    virtual void nextstart() { next(); }
    virtual void next() {}
    virtual void stop() {}
    
    // Notification methods
    virtual void notifyOrder(Order& order) { (void)order; }
    virtual void notifyTrade(Trade& trade) { (void)trade; }
    virtual void notifyCashValue(Value cash, Value value) { (void)cash; (void)value; }
    
    /**
     * @brief Get analysis results
     * @return Map of analysis name to value
     */
    std::map<std::string, Value> getAnalysis() const { return analysis_; }
    
    /**
     * @brief Create initial analysis structure
     */
    virtual void createAnalysis() {}
    
    // Setup
    void setStrategy(Strategy* s) { strategy_ = s; }
    void setBroker(Broker* b) { broker_ = b; }
    Strategy* strategy() const { return strategy_; }
    Broker* broker() const { return broker_; }

protected:
    Strategy* strategy_ = nullptr;
    Broker* broker_ = nullptr;
    std::map<std::string, Value> analysis_;
};

/**
 * @brief Trade Analyzer
 * 
 * Analyzes trades: total, won, lost, streaks, etc.
 */
class TradeAnalyzer : public Analyzer {
public:
    void start() override;
    void notifyTrade(Trade& trade) override;
    void stop() override;

private:
    Size totalTrades_ = 0;
    Size wonTrades_ = 0;
    Size lostTrades_ = 0;
    Value grossProfit_ = 0;
    Value grossLoss_ = 0;
    Size currentStreak_ = 0;
    Size maxWinStreak_ = 0;
    Size maxLossStreak_ = 0;
    bool lastWasWin_ = false;
};

/**
 * @brief Returns analyzer
 * 
 * Calculates various return metrics.
 */
class ReturnsAnalyzer : public Analyzer {
public:
    void start() override;
    void next() override;
    void stop() override;

private:
    Value startValue_ = 0;
    Value prevValue_ = 0;
    std::vector<Value> returns_;
};

/**
 * @brief Sharpe Ratio analyzer
 * 
 * Calculates the Sharpe Ratio of the strategy.
 */
class SharpeRatio : public Analyzer {
public:
    SharpeRatio() {
        // 手动设置默认参数
        params_.set("riskfreerate", 0.01);
        params_.set("annualize", true);
        params_.set("tradingdays", 252);
    }
    
    explicit SharpeRatio(const Params& p) : SharpeRatio() {
        params_.override(p);
    }
    
    // stddev_sample 参数通过构造函数传入
    bool useSampleStdDev() const { return useSampleStdDev_; }
    void setUseSampleStdDev(bool v) { useSampleStdDev_ = v; }
    
    void start() override;
    void next() override;
    void stop() override;

private:
    bool useSampleStdDev_ = false;
    Value startValue_ = 0;
    Value prevValue_ = 0;
    std::vector<Value> returns_;
};

/**
 * @brief Drawdown analyzer
 * 
 * Calculates drawdown statistics:
 * - Current drawdown (monetary and percentage)
 * - Maximum drawdown (monetary and percentage)
 * - Drawdown duration
 */
class DrawDown : public Analyzer {
public:
    void start() override;
    void next() override;
    void stop() override;

private:
    Value maxValue_ = 0;
    Value currentDrawdown_ = 0;
    Value currentDrawdownPct_ = 0;
    Value maxDrawdown_ = 0;
    Value maxDrawdownPct_ = 0;
    Size drawdownLen_ = 0;
    Size maxDrawdownLen_ = 0;
};

/**
 * @brief Annual Return analyzer
 * 
 * Calculates returns on an annual basis.
 */
class AnnualReturn : public Analyzer {
public:
    void start() override;
    void next() override;
    void stop() override;

private:
    Value startValue_ = 0;
    std::map<int, Value> yearlyReturns_;  // year -> return
    int currentYear_ = 0;
    Value yearStartValue_ = 0;
};

/**
 * @brief SQN (System Quality Number) analyzer
 * 
 * Van Tharp's System Quality Number.
 */
class SQN : public Analyzer {
public:
    void start() override;
    void notifyTrade(Trade& trade) override;
    void stop() override;

private:
    std::vector<Value> tradePnLs_;
};

} // namespace bt
