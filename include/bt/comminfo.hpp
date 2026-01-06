/**
 * @file comminfo.hpp
 * @brief Commission Information System
 * 
 * Corresponds to Python's comminfo.py
 * Provides comprehensive commission, margin, and interest calculations.
 */

#pragma once

#include "bt/common.hpp"
#include "bt/params.hpp"
#include <cmath>
#include <algorithm>
#include <optional>

namespace bt {

// Forward declarations
class DataFeed;

/**
 * @brief Commission calculation type
 */
enum class CommType {
    Percentage,  // Commission as percentage of trade value
    Fixed,       // Fixed commission per share/contract
    PerTrade     // Fixed commission per trade
};

/**
 * @brief Asset type for commission calculation
 */
enum class AssetType {
    Stock,       // Stock-like asset (no leverage, no margin)
    Futures,     // Futures contract (leverage, margin required)
    Options,     // Options contract
    Forex        // Forex pair
};

/**
 * @brief Base Commission Information class
 * 
 * Handles all aspects of commission and margin calculations:
 * - Commission costs (percentage, fixed, or per-trade)
 * - Margin requirements
 * - Interest calculations
 * - P&L calculations
 * - Leverage effects
 * 
 * This is the base class that can be customized for different brokers/assets.
 */
class CommInfo : public Parametrized<CommInfo> {
public:
    /**
     * @brief Parameters for commission calculation
     */
    struct Params {
        Value commission = 0.0;        ///< Commission rate/amount
        Value mult = 1.0;              ///< Contract multiplier (for futures)
        std::optional<Value> margin;   ///< Margin requirement (nullopt = no margin)
        bool automargin = false;       ///< Auto-calculate margin from price
        CommType commtype = CommType::Percentage; ///< Commission type
        bool stocklike = true;         ///< Stock-like (true) vs futures-like (false)
        bool percabs = false;          ///< Percentage as absolute (0.01 = 1%)
        Value interest = 0.0;          ///< Interest rate (annual, as decimal)
        bool interest_long = false;    ///< Charge interest on long positions
        Value leverage = 1.0;          ///< Leverage ratio
    };
    
    CommInfo() = default;
    virtual ~CommInfo() = default;
    
    // Parameter access
    Params& params() { return params_; }
    const Params& params() const { return params_; }
    
    // Setters for common parameters
    CommInfo& setCommission(Value c) { params_.commission = c; return *this; }
    CommInfo& setMult(Value m) { params_.mult = m; return *this; }
    CommInfo& setMargin(Value m) { params_.margin = m; return *this; }
    CommInfo& setAutoMargin(bool a) { params_.automargin = a; return *this; }
    CommInfo& setCommType(CommType t) { params_.commtype = t; return *this; }
    CommInfo& setStockLike(bool s) { params_.stocklike = s; return *this; }
    CommInfo& setPercAbs(bool p) { params_.percabs = p; return *this; }
    CommInfo& setInterest(Value i) { params_.interest = i; return *this; }
    CommInfo& setInterestLong(bool il) { params_.interest_long = il; return *this; }
    CommInfo& setLeverage(Value l) { params_.leverage = l; return *this; }
    
    // Getters
    Value commission() const { return params_.commission; }
    Value mult() const { return params_.mult; }
    bool hasMargin() const { return params_.margin.has_value(); }
    Value margin() const { return params_.margin.value_or(0.0); }
    bool automargin() const { return params_.automargin; }
    CommType commtype() const { return params_.commtype; }
    bool stocklike() const { return params_.stocklike; }
    bool percabs() const { return params_.percabs; }
    Value interest() const { return params_.interest; }
    bool interestLong() const { return params_.interest_long; }
    Value leverage() const { return params_.leverage; }
    
    /**
     * @brief Get effective commission rate
     * 
     * Converts the commission value to a percentage if needed.
     * - For percentage commission: returns as-is or divides by 100
     * - For fixed commission: returns 0 (not rate-based)
     */
    virtual Value getCommissionRate() const {
        if (params_.commtype != CommType::Percentage) {
            return 0.0;
        }
        if (params_.percabs) {
            return params_.commission;  // Already in 0.01 = 1% format
        }
        return params_.commission / 100.0;  // Convert 1 = 1%
    }
    
    /**
     * @brief Calculate margin requirement for a position
     * 
     * @param price Current price
     * @return Margin per share/contract
     */
    virtual Value getMargin(Value price) const {
        if (params_.automargin) {
            // Auto-margin: use price * multiplier / leverage
            return price * params_.mult / params_.leverage;
        }
        if (params_.margin.has_value()) {
            return params_.margin.value();
        }
        // No margin (stock-like)
        return price * params_.mult;
    }
    
    /**
     * @brief Calculate commission for a trade
     * 
     * @param size Number of shares/contracts
     * @param price Execution price
     * @return Commission amount
     */
    virtual Value getCommission(Value size, Value price) const {
        size = std::abs(size);
        
        switch (params_.commtype) {
            case CommType::Percentage: {
                Value rate = getCommissionRate();
                return size * price * params_.mult * rate;
            }
            case CommType::Fixed:
                return size * params_.commission;
            case CommType::PerTrade:
                return params_.commission;
            default:
                return 0.0;
        }
    }
    
    /**
     * @brief Get value of a position
     * 
     * @param size Position size (can be negative for short)
     * @param price Current price
     * @return Position value
     */
    virtual Value getValueSize(Value size, Value price) const {
        return size * price * params_.mult;
    }
    
    /**
     * @brief Get operation cost (value + commission)
     * 
     * @param size Number of shares/contracts
     * @param price Execution price
     * @return Total cost of operation
     */
    virtual Value getOperationCost(Value size, Value price) const {
        Value value = getValueSize(size, price);
        Value comm = getCommission(size, price);
        return std::abs(value) + comm;
    }
    
    /**
     * @brief Calculate maximum size that can be bought with given cash
     * 
     * @param price Current price
     * @param cash Available cash
     * @return Maximum position size
     */
    virtual Size getSize(Value price, Value cash) const {
        if (price <= 0) return 0;
        
        // For stock-like assets, need full value
        if (params_.stocklike) {
            Value effectivePrice = price * params_.mult;
            
            // Account for commission
            if (params_.commtype == CommType::Percentage) {
                Value rate = getCommissionRate();
                // cash = size * price * (1 + rate)
                // size = cash / (price * (1 + rate))
                effectivePrice *= (1.0 + rate);
            }
            
            Value size = cash / effectivePrice;
            
            // Subtract fixed commission if applicable
            if (params_.commtype == CommType::Fixed) {
                // Iteratively find max size accounting for commission
                while (size > 0) {
                    Value totalCost = size * price * params_.mult + size * params_.commission;
                    if (totalCost <= cash) break;
                    size -= 1;
                }
            }
            
            return static_cast<Size>(std::floor(std::max(0.0, size)));
        } else {
            // Futures-like: need margin only
            Value marginPerUnit = getMargin(price);
            if (marginPerUnit <= 0) return 0;
            
            Value size = cash / marginPerUnit;
            return static_cast<Size>(std::floor(std::max(0.0, size)));
        }
    }
    
    /**
     * @brief Calculate profit and loss for a position
     * 
     * @param size Position size
     * @param price Entry price
     * @param newprice Current/exit price
     * @return P&L amount
     */
    virtual Value profitAndLoss(Value size, Value price, Value newprice) const {
        return size * params_.mult * (newprice - price);
    }
    
    /**
     * @brief Cash adjustment when opening a position
     * 
     * For stocks: returns negative value (cash goes down)
     * For futures: returns 0 (only margin is locked)
     * 
     * @param size Position size
     * @param price Entry price
     * @return Cash adjustment
     */
    virtual Value cashAdjustOpen(Value size, Value price) const {
        if (params_.stocklike) {
            // Stock: need to pay full value
            return -size * price * params_.mult;
        }
        // Futures: only margin is locked, cash stays
        return 0.0;
    }
    
    /**
     * @brief Cash adjustment when closing a position
     * 
     * @param size Position size (negative for close sell)
     * @param price Entry/average price
     * @param newprice Exit price
     * @return Cash adjustment (positive = cash received)
     */
    virtual Value cashAdjustClose(Value size, Value price, Value newprice) const {
        if (params_.stocklike) {
            // Stock: receive full value
            return -size * newprice * params_.mult;
        }
        // Futures: receive P&L
        return profitAndLoss(size, price, newprice);
    }
    
    /**
     * @brief Calculate interest for holding a position
     * 
     * @param size Position size
     * @param price Current price
     * @param days Number of days held
     * @return Interest amount (positive = cost)
     */
    virtual Value getInterest(Value size, Value price, int days) const {
        if (params_.interest == 0.0) return 0.0;
        
        // Check if we should charge interest
        bool isLong = size > 0;
        if (isLong && !params_.interest_long) return 0.0;
        
        // Calculate daily interest
        Value posValue = std::abs(size * price * params_.mult);
        Value dailyRate = params_.interest / 365.0;
        
        return posValue * dailyRate * days;
    }
    
    /**
     * @brief Confirm execution and update internal state
     * 
     * Called after an order is executed. Can be overridden for
     * custom tracking.
     * 
     * @param size Executed size
     * @param price Execution price
     */
    virtual void confirmExec(Value size, Value price) {
        (void)size; (void)price;
        // Base implementation does nothing
        // Override for custom behavior
    }

protected:
    Params params_;
};

/**
 * @brief Commission info for stocks
 * 
 * Pre-configured for typical stock trading:
 * - Percentage-based commission
 * - No margin requirement
 * - No leverage
 */
class CommInfoStock : public CommInfo {
public:
    CommInfoStock(Value commission = 0.001, bool percabs = true) {
        params_.commission = commission;
        params_.percabs = percabs;
        params_.commtype = CommType::Percentage;
        params_.stocklike = true;
        params_.mult = 1.0;
        params_.leverage = 1.0;
    }
};

/**
 * @brief Commission info for futures
 * 
 * Pre-configured for typical futures trading:
 * - Fixed commission per contract
 * - Margin requirement
 * - Multiplier for contract size
 */
class CommInfoFutures : public CommInfo {
public:
    CommInfoFutures(Value commission = 2.0, Value margin = 2000.0, Value mult = 50.0) {
        params_.commission = commission;
        params_.commtype = CommType::Fixed;
        params_.stocklike = false;
        params_.margin = margin;
        params_.mult = mult;
    }
};

/**
 * @brief Commission info for forex
 * 
 * Pre-configured for forex trading:
 * - Spread-based (no explicit commission)
 * - High leverage
 * - Interest (swap) rates
 */
class CommInfoForex : public CommInfo {
public:
    CommInfoForex(Value leverage = 100.0, Value interest = 0.0) {
        params_.commission = 0.0;
        params_.commtype = CommType::Fixed;
        params_.stocklike = false;
        params_.leverage = leverage;
        params_.automargin = true;
        params_.interest = interest;
        params_.interest_long = true;  // Forex charges interest both ways
        params_.mult = 1.0;
    }
};

/**
 * @brief Commission info for options
 * 
 * Pre-configured for options trading:
 * - Fixed commission per contract
 * - Multiplier of 100 (standard equity option)
 */
class CommInfoOptions : public CommInfo {
public:
    CommInfoOptions(Value commission = 0.65, Value mult = 100.0) {
        params_.commission = commission;
        params_.commtype = CommType::Fixed;
        params_.stocklike = true;
        params_.mult = mult;
    }
};

/**
 * @brief Interactive Brokers-style commission
 * 
 * Tiered commission structure with minimums and maximums.
 */
class CommInfoIB : public CommInfo {
public:
    struct IBParams {
        Value perShare = 0.005;     // Per share rate
        Value minPerOrder = 1.0;    // Minimum per order
        Value maxPercentage = 0.5;  // Max as % of trade value (0.5%)
    };
    
    CommInfoIB() {
        params_.commtype = CommType::Fixed;  // We override getCommission
        params_.stocklike = true;
    }
    
    IBParams& ibParams() { return ibParams_; }
    const IBParams& ibParams() const { return ibParams_; }
    
    Value getCommission(Value size, Value price) const override {
        size = std::abs(size);
        
        // Per-share commission
        Value comm = size * ibParams_.perShare;
        
        // Apply minimum
        comm = std::max(comm, ibParams_.minPerOrder);
        
        // Apply maximum (% of trade value)
        Value tradeValue = size * price * params_.mult;
        Value maxComm = tradeValue * (ibParams_.maxPercentage / 100.0);
        comm = std::min(comm, maxComm);
        
        return comm;
    }

private:
    IBParams ibParams_;
};

/**
 * @brief Per-trade flat fee commission
 * 
 * Simple flat fee per trade regardless of size.
 * Common with many retail brokers (e.g., $0 or $4.95 per trade).
 */
class CommInfoFlat : public CommInfo {
public:
    explicit CommInfoFlat(Value feePerTrade = 0.0) {
        params_.commission = feePerTrade;
        params_.commtype = CommType::PerTrade;
        params_.stocklike = true;
    }
};

/**
 * @brief Commission scheme with different rates for buy/sell
 * 
 * Some brokers charge different rates for buying vs selling.
 */
class CommInfoBuySell : public CommInfo {
public:
    CommInfoBuySell(Value buyRate = 0.001, Value sellRate = 0.001, 
                    bool percabs = true) 
        : buyRate_(buyRate), sellRate_(sellRate) {
        params_.commtype = CommType::Percentage;
        params_.percabs = percabs;
        params_.stocklike = true;
    }
    
    Value getCommission(Value size, Value price) const override {
        Value rate = (size >= 0) ? buyRate_ : sellRate_;
        if (!params_.percabs) {
            rate /= 100.0;
        }
        return std::abs(size) * price * params_.mult * rate;
    }

private:
    Value buyRate_;
    Value sellRate_;
};

} // namespace bt
