/**
 * @file observer.hpp
 * @brief Observer System
 * 
 * Corresponds to Python's observer.py and observers/
 * Observers track values over time and store them in lines for plotting.
 */

#pragma once

#include "bt/lineseries.hpp"
#include "bt/params.hpp"
#include "bt/order.hpp"
#include <vector>

namespace bt {

class Strategy;
class Broker;
class DataFeed;

/**
 * @brief Base Observer class
 * 
 * Observers are Lines objects that track values during the backtest.
 * Unlike analyzers which compute final statistics, observers store
 * values for each bar (useful for plotting).
 */
class Observer : public LineSeries, public Parametrized<Observer> {
public:
    virtual ~Observer() = default;
    
    // Lifecycle methods
    virtual void start() {}
    virtual void prenext() { next(); }  // Default: always observe
    virtual void nextstart() { next(); }
    virtual void next() = 0;
    virtual void stop() {}
    
    // Setup
    void setStrategy(Strategy* s) { strategy_ = s; }
    void setBroker(Broker* b) { broker_ = b; }
    Strategy* strategy() const { return strategy_; }
    Broker* broker() const { return broker_; }

protected:
    Strategy* strategy_ = nullptr;
    Broker* broker_ = nullptr;
};

/**
 * @brief Cash observer
 * 
 * Tracks current cash amount in the broker.
 */
class CashObserver : public Observer {
public:
    CashObserver() { 
        addLine("cash"); 
    }
    
    void next() override;
    
    LineBuffer& cash() { return line(0); }
    const LineBuffer& cash() const { return line(0); }
};

/**
 * @brief Value observer
 * 
 * Tracks portfolio value (cash + positions) in the broker.
 */
class ValueObserver : public Observer {
public:
    ValueObserver() { 
        addLine("value"); 
    }
    
    void next() override;
    
    LineBuffer& value() { return line(0); }
    const LineBuffer& value() const { return line(0); }
};

/**
 * @brief Combined broker observer (cash + value)
 * 
 * Tracks both cash and portfolio value.
 */
class BrokerObserver : public Observer {
public:
    BrokerObserver() { 
        addLine("cash"); 
        addLine("value"); 
    }
    
    void next() override;
    
    LineBuffer& cash() { return line(0); }
    LineBuffer& value() { return line(1); }
};

/**
 * @brief Drawdown observer
 * 
 * Tracks current drawdown and maximum drawdown.
 */
class DrawDownObserver : public Observer {
public:
    DrawDownObserver() { 
        addLine("drawdown"); 
        addLine("maxdrawdown"); 
    }
    
    void start() override;
    void next() override;
    
    LineBuffer& drawdown() { return line(0); }
    LineBuffer& maxdrawdown() { return line(1); }

private:
    Value maxValue_ = 0;
};

/**
 * @brief Buy/Sell observer
 * 
 * Marks buy and sell points on the chart.
 * Stores execution prices (buy positive, sell negative).
 */
class BuySellObserver : public Observer {
public:
    BuySellObserver() {
        addLine("buy");
        addLine("sell");
    }
    
    void next() override;
    void notifyOrder(Order& order);
    
    LineBuffer& buy() { return line(0); }
    LineBuffer& sell() { return line(1); }

private:
    std::vector<Order*> pendingOrders_;
};

/**
 * @brief Trades observer
 * 
 * Marks trade profits/losses on the chart.
 */
class TradesObserver : public Observer {
public:
    TradesObserver() {
        addLine("pnl");
        addLine("pnlcomm");
    }
    
    void next() override;
    void notifyTrade(Trade& trade);
    
    LineBuffer& pnl() { return line(0); }
    LineBuffer& pnlcomm() { return line(1); }

private:
    std::vector<Trade*> pendingTrades_;
};

/**
 * @brief Returns observer
 * 
 * Tracks period returns (daily by default).
 */
class ReturnsObserver : public Observer {
public:
    ReturnsObserver() {
        addLine("returns");
    }
    
    void start() override;
    void next() override;
    
    LineBuffer& returns() { return line(0); }

private:
    Value prevValue_ = 0;
};

/**
 * @brief Log returns observer
 * 
 * Tracks logarithmic returns.
 */
class LogReturnsObserver : public Observer {
public:
    LogReturnsObserver() {
        addLine("logreturns");
    }
    
    void start() override;
    void next() override;
    
    LineBuffer& logreturns() { return line(0); }

private:
    Value prevValue_ = 0;
};

} // namespace bt
