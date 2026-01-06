/**
 * @file strategy.hpp
 * @brief Strategy Framework
 * 
 * Corresponds to Python's strategy.py
 * The Strategy class is the base class for all trading strategies.
 */

#pragma once

#include "bt/params.hpp"
#include "bt/order.hpp"
#include "bt/indicator.hpp"
#include "bt/signal.hpp"
#include "bt/timer.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <array>

namespace bt {

// Forward declarations
class DataFeed;
class Broker;
class Cerebro;

/**
 * @brief Base Strategy class
 * 
 * Users should subclass this and override the lifecycle methods
 * (especially `next()`) to implement their trading logic.
 * 
 * Lifecycle:
 * 1. __init__ (C++: constructor + init())
 * 2. start() - Called before backtesting begins
 * 3. prenext() - Called while indicators warm up
 * 4. nextstart() - Called once when indicators ready
 * 5. next() - Called for each bar
 * 6. stop() - Called after backtesting ends
 */
class Strategy : public Parametrized<Strategy> {
public:
    virtual ~Strategy() = default;
    
    // ==================== Lifecycle Methods ====================
    
    /**
     * @brief Initialize indicators (called once after construction)
     */
    virtual void init() {}
    
    /**
     * @brief Called before backtesting begins
     */
    virtual void start() {}
    
    /**
     * @brief Called while indicators reach minimum period
     */
    virtual void prenext() {}
    
    /**
     * @brief Called exactly once when all indicators are ready
     */
    virtual void nextstart() { next(); }
    
    /**
     * @brief Called for each bar after indicators are ready
     */
    virtual void next() {}
    
    /**
     * @brief Called after backtesting ends
     */
    virtual void stop() {}
    
    // ==================== Cheat-on-Open Methods ====================
    
    /**
     * @brief Called during prenext phase before bar processing (cheat-on-open)
     */
    virtual void prenextOpen() {}
    
    /**
     * @brief Called once when indicators ready, before bar processing (cheat-on-open)
     */
    virtual void nextstartOpen() { nextOpen(); }
    
    /**
     * @brief Called for each bar before bar processing (cheat-on-open)
     * 
     * This method is called at the open of the bar, allowing strategies
     * to place orders that execute at the open price.
     */
    virtual void nextOpen() {}
    
    // ==================== Notification Methods ====================
    
    /**
     * @brief Called when order state changes
     */
    virtual void notifyOrder(Order& order) { (void)order; }
    
    /**
     * @brief Called when a trade is opened/closed
     */
    virtual void notifyTrade(Trade& trade) { (void)trade; }
    
    /**
     * @brief Called with cash and value updates
     */
    virtual void notifyCashValue(Value cash, Value value) { (void)cash; (void)value; }
    
    /**
     * @brief Called with data feed notifications
     */
    virtual void notifyData(DataFeed* data, int status) { (void)data; (void)status; }
    
    /**
     * @brief Called with fund value updates (fund mode)
     * 
     * @param cash Current cash
     * @param value Portfolio value
     * @param fundvalue Value per fund share
     * @param shares Number of fund shares
     */
    virtual void notifyFund(Value cash, Value value, Value fundvalue, Value shares) {
        (void)cash; (void)value; (void)fundvalue; (void)shares;
    }
    
    /**
     * @brief Called with store notifications
     * 
     * @param msg Notification message
     */
    virtual void notifyStore(const std::string& msg) { (void)msg; }
    
    /**
     * @brief Called when a timer triggers
     * 
     * @param timer The timer that triggered
     * @param when The time it triggered
     */
    virtual void notifyTimer(const Timer& timer, const DateTime& when) {
        (void)timer; (void)when;
    }
    
    // ==================== Trading Methods ====================
    
    /**
     * @brief Create a buy order
     * @param data Data feed (nullptr = first data)
     * @param size Order size (0 = use sizer)
     * @param price Limit price (0 = market order)
     * @param exectype Execution type
     * @return Pointer to created order
     */
    Order* buy(DataFeed* data = nullptr, Size size = 0, Value price = 0,
               OrderType exectype = OrderType::Market);
    
    /**
     * @brief Create a sell order
     */
    Order* sell(DataFeed* data = nullptr, Size size = 0, Value price = 0,
                OrderType exectype = OrderType::Market);
    
    /**
     * @brief Close current position
     */
    Order* closePosition(DataFeed* data = nullptr, Size size = 0);
    
    /**
     * @brief Cancel an order
     */
    void cancel(Order* order);
    
    // ==================== Order Target Methods ====================
    
    /**
     * @brief Place order to reach target position size
     * 
     * If current position is 10 and target is 15, creates buy order for 5.
     * If current position is 10 and target is 5, creates sell order for 5.
     * If current position is 10 and target is -5, creates sell order for 15.
     * 
     * @param data Data feed (nullptr = first data)
     * @param target Target position size (positive = long, negative = short)
     * @param price Limit price (0 = market)
     * @param exectype Order execution type
     * @return Order pointer or nullptr if no action needed
     */
    Order* orderTargetSize(DataFeed* data = nullptr, Value target = 0,
                           Value price = 0, OrderType exectype = OrderType::Market);
    
    /**
     * @brief Place order to reach target portfolio value
     * 
     * Calculates the number of shares needed to have a position worth
     * the target value at current prices.
     * 
     * @param data Data feed (nullptr = first data)
     * @param target Target value of position
     * @param price Limit price (0 = market)
     * @param exectype Order execution type
     * @return Order pointer or nullptr if no action needed
     */
    Order* orderTargetValue(DataFeed* data = nullptr, Value target = 0,
                            Value price = 0, OrderType exectype = OrderType::Market);
    
    /**
     * @brief Place order to reach target percentage of portfolio
     * 
     * Target is expressed as percentage of total portfolio value.
     * 
     * @param data Data feed (nullptr = first data)
     * @param target Target percentage (e.g., 50.0 for 50%)
     * @param price Limit price (0 = market)
     * @param exectype Order execution type
     * @return Order pointer or nullptr if no action needed
     */
    Order* orderTargetPercent(DataFeed* data = nullptr, Value target = 0,
                              Value price = 0, OrderType exectype = OrderType::Market);
    
    // ==================== Bracket Order Methods ====================
    
    /**
     * @brief Bracket order configuration
     */
    struct BracketConfig {
        Value size = 0;
        Value price = 0;           // Main order price
        Value stopPrice = 0;       // Stop loss price
        Value limitPrice = 0;      // Take profit price
        Value trailAmount = 0;     // Trail amount for stop
        Value trailPercent = 0;    // Trail percent for stop
        OrderType exectype = OrderType::Limit;
        OrderType stopExec = OrderType::Stop;
        OrderType limitExec = OrderType::Limit;
        int tradeId = 0;
        double valid = 0;          // Valid until datetime
    };
    
    /**
     * @brief Create a bracket buy order
     * 
     * Creates three orders:
     * 1. Main buy order
     * 2. Stop loss sell order (low side)
     * 3. Take profit sell order (high side)
     * 
     * @return Array of [main order, stop order, limit order]
     */
    std::array<Order*, 3> buyBracket(DataFeed* data = nullptr,
                                     const BracketConfig& config = {});
    
    /**
     * @brief Create a bracket sell order
     * 
     * Creates three orders:
     * 1. Main sell order
     * 2. Stop loss buy order (high side)
     * 3. Take profit buy order (low side)
     * 
     * @return Array of [main order, stop order, limit order]
     */
    std::array<Order*, 3> sellBracket(DataFeed* data = nullptr,
                                      const BracketConfig& config = {});
    
    // ==================== Signal Methods ====================
    
    /**
     * @brief Add a signal to the strategy
     * 
     * @param signal The signal indicator
     * @param signalType The type of signal
     * @param dataIndex The data feed index this signal applies to
     */
    void addSignal(Signal* signal, SignalType signalType, Size dataIndex = 0) {
        signals_.addSignal(signal, signalType, dataIndex);
    }
    
    /**
     * @brief Get the signal group
     */
    const SignalGroup& signals() const { return signals_; }
    SignalGroup& signals() { return signals_; }
    
    /**
     * @brief Check if there's a long entry signal
     */
    bool hasLongSignal() const { return signals_.hasLongEntry(); }
    
    /**
     * @brief Check if there's a short entry signal
     */
    bool hasShortSignal() const { return signals_.hasShortEntry(); }
    
    /**
     * @brief Check if there's a long exit signal
     */
    bool hasLongExitSignal() const { return signals_.hasLongExit(); }
    
    /**
     * @brief Check if there's a short exit signal
     */
    bool hasShortExitSignal() const { return signals_.hasShortExit(); }
    
    // ==================== Timer Methods ====================
    
    /**
     * @brief Add a timer to the strategy
     * 
     * @param when Time of day to trigger
     * @param offsetMinutes Offset in minutes (can be negative)
     * @param repeatMinutes Repeat interval (0 = no repeat)
     * @return Timer ID
     */
    int addTimer(const TimeOfDay& when, int offsetMinutes = 0, int repeatMinutes = 0) {
        return timerManager_.addTimer(when, offsetMinutes, repeatMinutes);
    }
    
    /**
     * @brief Add a timer with full configuration
     * @param timer Timer object
     * @return Timer ID
     */
    int addTimer(Timer timer) {
        return timerManager_.addTimer(std::move(timer));
    }
    
    /**
     * @brief Get timer manager
     */
    TimerManager& timerManager() { return timerManager_; }
    const TimerManager& timerManager() const { return timerManager_; }
    
    // ==================== Position Methods ====================
    
    /**
     * @brief Get current position size
     */
    Value getPosition(DataFeed* data = nullptr) const;
    
    /**
     * @brief Convenience: position for first data
     */
    Value position() const { return getPosition(nullptr); }
    
    // ==================== Data Access ====================
    
    /**
     * @brief Get data feed by index
     */
    DataFeed* data(Size idx = 0) const {
        return idx < datas_.size() ? datas_[idx] : nullptr;
    }
    
    /**
     * @brief Shorthand for data(0)
     */
    DataFeed* data0() const { return data(0); }
    
    /**
     * @brief Get number of data feeds
     */
    Size dataCount() const { return datas_.size(); }
    
    /**
     * @brief Get data feed by name
     */
    DataFeed* getDataByName(const std::string& name) const;
    
    /**
     * @brief Get data name
     */
    const std::string& getDataName(Size idx = 0) const {
        static std::string empty;
        return idx < dataNames_.size() ? dataNames_[idx] : empty;
    }
    
    // ==================== Broker Access ====================
    
    /**
     * @brief Get broker reference
     */
    Broker* getBroker() const { return broker_; }
    
    // ==================== Setup (called by Cerebro) ====================
    
    void setCerebro(Cerebro* c) { cerebro_ = c; }
    void setBroker(Broker* b) { broker_ = b; }
    void addData(DataFeed* d, const std::string& name = "") {
        datas_.push_back(d);
        dataNames_.push_back(name.empty() ? "data" + std::to_string(datas_.size()-1) : name);
    }
    
    // Period management
    void setMinPeriod(Size p) { minPeriod_ = p; }
    Size minPeriod() const { return minPeriod_; }
    void updateMinPeriod(Size p) { if (p > minPeriod_) minPeriod_ = p; }
    
    // Bar tracking
    void setBarIndex(Size idx) { barIndex_ = idx; }
    Size barIndex() const { return barIndex_; }
    void setBarLength(Size len) { barLength_ = len; }
    Size barLength() const { return barLength_; }
    
    // Indicator management
    void addIndicator(Indicator* ind) { indicators_.push_back(ind); }
    const std::vector<Indicator*>& indicators() const { return indicators_; }
    
    // Sizing
    void setSizer(std::function<Size(DataFeed*, bool)> sizer) { sizer_ = sizer; }
    Size getSizing(DataFeed* data = nullptr, bool isbuy = true) const {
        if (sizer_) return sizer_(data ? data : datas_[0], isbuy);
        return 1;  // Default fixed size
    }

protected:
    Cerebro* cerebro_ = nullptr;
    Broker* broker_ = nullptr;
    std::vector<DataFeed*> datas_;
    std::vector<std::string> dataNames_;
    std::vector<Indicator*> indicators_;
    SignalGroup signals_;
    TimerManager timerManager_;
    Size minPeriod_ = 1;
    Size barIndex_ = 0;
    Size barLength_ = 0;
    std::function<Size(DataFeed*, bool)> sizer_;
};

} // namespace bt
