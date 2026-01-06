/**
 * @file broker.hpp
 * @brief Broker System
 * 
 * Corresponds to Python's brokers/backtesting.py
 * Enhanced broker with slippage, filler, fund mode, cheat modes.
 */

#pragma once

#include "bt/order.hpp"
#include "bt/comminfo.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace bt {

class DataFeed;

// Keep old CommInfoBase for backward compatibility
using CommInfoBase = CommInfo;

/**
 * @brief Slippage configuration
 */
struct SlippageConfig {
    Value perc = 0.0;           ///< Slippage as percentage of price
    Value fixed = 0.0;          ///< Fixed slippage amount
    bool slipOpen = false;      ///< Apply slippage to open orders
    bool slipMatch = true;      ///< Apply slippage when matching high/low
    bool slipLimit = true;      ///< Apply slippage to limit orders
    bool slipOut = false;       ///< Allow slippage outside bar range
};

/**
 * @brief Volume filler interface
 * 
 * Determines how much of an order can be filled based on available volume.
 */
class VolumeFiller {
public:
    virtual ~VolumeFiller() = default;
    
    /**
     * @brief Calculate fillable volume
     * 
     * @param order The order to fill
     * @param price Execution price
     * @param volume Available volume
     * @return Size that can be filled
     */
    virtual Size fill(const Order& order, Value price, Value volume) = 0;
};

/**
 * @brief Default filler - fills entire order regardless of volume
 */
class DefaultFiller : public VolumeFiller {
public:
    Size fill(const Order& order, Value, Value) override {
        return std::abs(order.size());
    }
};

/**
 * @brief Volume-limited filler - fills up to bar volume
 */
class BarVolumeFiller : public VolumeFiller {
public:
    explicit BarVolumeFiller(Value maxPercent = 100.0) 
        : maxPercent_(maxPercent / 100.0) {}
    
    Size fill(const Order& order, Value, Value volume) override {
        Size maxFill = static_cast<Size>(volume * maxPercent_);
        Size orderSize = static_cast<Size>(std::abs(order.size()));
        return (orderSize < maxFill) ? orderSize : maxFill;
    }

private:
    Value maxPercent_;
};

/**
 * @brief Fixed volume filler - fills up to fixed amount
 */
class FixedVolumeFiller : public VolumeFiller {
public:
    explicit FixedVolumeFiller(Size maxSize) : maxSize_(maxSize) {}
    
    Size fill(const Order& order, Value, Value) override {
        Size orderSize = static_cast<Size>(std::abs(order.size()));
        return (orderSize < maxSize_) ? orderSize : maxSize_;
    }

private:
    Size maxSize_;
};

/**
 * @brief Broker class
 * 
 * Simulates a broker for backtesting with features:
 * - Order execution (market, limit, stop, etc.)
 * - Slippage simulation
 * - Volume-based filling
 * - Cheat-on-close/open modes
 * - Fund mode for NAV tracking
 */
class Broker {
public:
    using OrderCallback = std::function<void(Order&)>;
    using TradeCallback = std::function<void(Trade&)>;
    
    /**
     * @brief Broker parameters
     */
    struct Params {
        Value cash = 100000.0;      ///< Starting cash
        bool checksubmit = true;    ///< Check margin before submit
        bool eosbar = false;        ///< End-of-session bar handling
        bool coc = false;           ///< Cheat-On-Close
        bool coo = false;           ///< Cheat-On-Open
        bool int2pnl = true;        ///< Add interest to PnL
        bool shortcash = true;      ///< Short positions generate cash
        Value fundstartval = 100.0; ///< Initial fund value
        bool fundmode = false;      ///< Fund mode (track NAV)
        SlippageConfig slip;        ///< Slippage config
    };
    
    explicit Broker(Value cash = 100000.0) {
        params_.cash = cash;
        cash_ = cash;
        startCash_ = cash;
        filler_ = std::make_unique<DefaultFiller>();
    }
    
    // Parameters
    Params& params() { return params_; }
    const Params& params() const { return params_; }
    
    // ==================== Slippage Configuration ====================
    
    /**
     * @brief Set percentage-based slippage
     */
    void setSlippagePerc(Value perc, bool slipOpen = false, 
                         bool slipMatch = true, bool slipLimit = true,
                         bool slipOut = false) {
        params_.slip.perc = perc;
        params_.slip.fixed = 0;
        params_.slip.slipOpen = slipOpen;
        params_.slip.slipMatch = slipMatch;
        params_.slip.slipLimit = slipLimit;
        params_.slip.slipOut = slipOut;
    }
    
    /**
     * @brief Set fixed slippage
     */
    void setSlippageFixed(Value fixed, bool slipOpen = false,
                          bool slipMatch = true, bool slipLimit = true,
                          bool slipOut = false) {
        params_.slip.fixed = fixed;
        params_.slip.perc = 0;
        params_.slip.slipOpen = slipOpen;
        params_.slip.slipMatch = slipMatch;
        params_.slip.slipLimit = slipLimit;
        params_.slip.slipOut = slipOut;
    }
    
    // ==================== Filler Configuration ====================
    
    /**
     * @brief Set volume filler
     */
    void setFiller(std::unique_ptr<VolumeFiller> filler) {
        filler_ = std::move(filler);
    }
    
    // ==================== Cheat Modes ====================
    
    /**
     * @brief Enable/disable Cheat-On-Close
     */
    void setCOC(bool coc) { params_.coc = coc; }
    bool isCOC() const { return params_.coc; }
    
    /**
     * @brief Enable/disable Cheat-On-Open
     */
    void setCOO(bool coo) { params_.coo = coo; }
    bool isCOO() const { return params_.coo; }
    
    // ==================== Fund Mode ====================
    
    /**
     * @brief Enable/disable fund mode
     */
    void setFundMode(bool mode, Value startval = 100.0) {
        params_.fundmode = mode;
        params_.fundstartval = startval;
        if (mode) {
            fundShares_ = cash_ / startval;
            fundValue_ = startval;
        }
    }
    
    bool isFundMode() const { return params_.fundmode; }
    Value getFundShares() const { return fundShares_; }
    Value getFundValue() const { return fundValue_; }
    
    // ==================== Account ====================
    
    void setCash(Value c) { cash_ = c; startCash_ = c; }
    void addCash(Value c) { cash_ += c; }
    Value getCash() const { return cash_; }
    Value getStartCash() const { return startCash_; }
    Value getValue() const;
    
    // ==================== Position ====================
    
    Value getPosition(const std::string& data) const {
        auto it = positions_.find(data);
        return it != positions_.end() ? it->second.size : 0;
    }
    
    Value getPositionPrice(const std::string& data) const {
        auto it = positions_.find(data);
        return it != positions_.end() ? it->second.price : 0;
    }
    
    Value getPositionValue(const std::string& data) const;
    
    // ==================== Orders ====================
    
    Order* buy(const std::string& data, Size size, Value price = 0,
               OrderType type = OrderType::Market);
    Order* sell(const std::string& data, Size size, Value price = 0,
                OrderType type = OrderType::Market);
    void cancel(Size orderId);
    
    // ==================== Data ====================
    
    void addDataFeed(const std::string& name, DataFeed* feed) {
        dataFeeds_[name] = feed;
    }
    
    // ==================== Commission ====================
    
    void setCommission(std::shared_ptr<CommInfo> c) { commInfo_ = c; }
    
    /**
     * @brief Set commission info for specific data feed
     */
    void setCommissionInfo(const std::string& data, std::shared_ptr<CommInfo> c) {
        commInfoMap_[data] = c;
    }
    
    CommInfo* getCommissionInfo(const std::string& data) const {
        auto it = commInfoMap_.find(data);
        if (it != commInfoMap_.end()) return it->second.get();
        return commInfo_ ? commInfo_.get() : nullptr;
    }
    
    // ==================== Callbacks ====================
    
    void setOrderCallback(OrderCallback cb) { orderCb_ = std::move(cb); }
    void setTradeCallback(TradeCallback cb) { tradeCb_ = std::move(cb); }
    
    // ==================== Processing ====================
    
    /**
     * @brief Process bar (normal mode)
     */
    void next();
    
    /**
     * @brief Process bar in cheat-on-open mode
     */
    void nextOpen();
    
    /**
     * @brief Process bar in cheat-on-close mode
     */
    void nextClose();
    
    // ==================== Reset ====================
    
    void reset() {
        cash_ = startCash_;
        positions_.clear();
        orders_.clear();
        trades_.clear();
        orderId_ = 0;
        fundShares_ = 0;
        fundValue_ = params_.fundstartval;
    }
    
    const std::vector<Trade>& getTrades() const { return trades_; }
    const std::vector<std::unique_ptr<Order>>& getOrders() const { return orders_; }

private:
    struct PositionInfo {
        Value size = 0;
        Value price = 0;
    };
    
    Params params_;
    Value cash_;
    Value startCash_;
    Size orderId_ = 0;
    
    // Fund mode tracking
    Value fundShares_ = 0;
    Value fundValue_ = 100.0;
    
    std::unordered_map<std::string, DataFeed*> dataFeeds_;
    std::unordered_map<std::string, PositionInfo> positions_;
    std::vector<std::unique_ptr<Order>> orders_;
    std::vector<Trade> trades_;
    
    std::shared_ptr<CommInfo> commInfo_;
    std::unordered_map<std::string, std::shared_ptr<CommInfo>> commInfoMap_;
    std::unique_ptr<VolumeFiller> filler_;
    OrderCallback orderCb_;
    TradeCallback tradeCb_;
    
    bool tryExecute(Order& order, bool atOpen = false, bool atClose = false);
    void executeOrder(Order& order, Value price, Size fillSize);
    
    /**
     * @brief Apply slippage to price
     */
    Value applySlippage(Value price, bool isBuy, const SlippageConfig& slip) {
        Value slipAmount = 0;
        
        if (slip.perc > 0) {
            slipAmount = price * slip.perc;
        } else if (slip.fixed > 0) {
            slipAmount = slip.fixed;
        }
        
        // Slippage goes against the trader
        if (isBuy) {
            return price + slipAmount;
        } else {
            return price - slipAmount;
        }
    }
};

} // namespace bt
