/**
 * @file sizer.hpp
 * @brief Position Sizing System
 * 
 * Corresponds to Python's sizer.py and sizers/
 * Provides position sizing calculation for strategy orders.
 * 
 * The Sizer determines how many units/shares to trade based on:
 * - Available cash
 * - Commission costs
 * - Risk parameters
 * - Current portfolio state
 */

#pragma once

#include "bt/params.hpp"
#include "bt/common.hpp"
#include <cmath>
#include <algorithm>

namespace bt {

// Forward declarations
class Strategy;
class Broker;
class DataFeed;
class CommInfo;

/**
 * @brief Base Sizer class
 * 
 * Sizers calculate the stake (number of shares/contracts) for an order.
 * They are called by the strategy when placing orders without explicit size.
 * 
 * Lifecycle:
 * - Attached to strategy via Cerebro.addSizer()
 * - Called during buy/sell operations when size is not specified
 */
class Sizer : public Parametrized<Sizer> {
public:
    virtual ~Sizer() = default;
    
    /**
     * @brief Calculate position size
     * 
     * @param comminfo Commission info for the data
     * @param cash Available cash
     * @param data Data feed being traded
     * @param isbuy True for buy, false for sell
     * @return Number of shares/contracts to trade
     */
    virtual Size getSizing(const CommInfo& comminfo, Value cash, 
                           const DataFeed& data, bool isbuy) = 0;
    
    /**
     * @brief Internal method called by strategy
     * 
     * This wraps getSizing and handles the reversal logic for sells
     * when position is being reversed.
     */
    Size _getSizing(const CommInfo& comminfo, Value cash,
                    const DataFeed& data, bool isbuy) {
        return getSizing(comminfo, cash, data, isbuy);
    }
    
    // Setup methods
    void setStrategy(Strategy* s) { strategy_ = s; }
    void setBroker(Broker* b) { broker_ = b; }
    Strategy* strategy() const { return strategy_; }
    Broker* broker() const { return broker_; }

protected:
    Strategy* strategy_ = nullptr;
    Broker* broker_ = nullptr;
};

/**
 * @brief Fixed size sizer
 * 
 * Returns a fixed stake (number of shares) regardless of account size.
 * 
 * Params:
 * - stake: Number of shares to buy/sell (default: 1)
 */
class FixedSizer : public Sizer {
public:
    struct Params {
        Size stake = 1;
    };
    
    FixedSizer() = default;
    explicit FixedSizer(Size stake) { params_.stake = stake; }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override {
        (void)comminfo; (void)cash; (void)data; (void)isbuy;
        return params_.stake;
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

// Alias for compatibility
using SizerFix = FixedSizer;

/**
 * @brief Fixed reverser sizer
 * 
 * Similar to FixedSizer but doubles the stake when reversing a position.
 * Used when going from long to short or vice versa.
 * 
 * Params:
 * - stake: Base number of shares (default: 1)
 */
class FixedReverser : public Sizer {
public:
    struct Params {
        Size stake = 1;
    };
    
    FixedReverser() = default;
    explicit FixedReverser(Size stake) { params_.stake = stake; }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Percent of portfolio sizer
 * 
 * Calculates position size as a percentage of available cash.
 * 
 * Params:
 * - percents: Percentage of cash to use (default: 20)
 */
class PercentSizer : public Sizer {
public:
    struct Params {
        Value percents = 20.0;  // Percentage of cash
    };
    
    PercentSizer() = default;
    explicit PercentSizer(Value percents) { params_.percents = percents; }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Percent of portfolio sizer (integer version)
 * 
 * Same as PercentSizer but always returns an integer (truncated) size.
 * 
 * Params:
 * - percents: Percentage of cash to use (default: 20)
 */
class PercentSizerInt : public PercentSizer {
public:
    using PercentSizer::PercentSizer;
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override {
        Value size = PercentSizer::getSizing(comminfo, cash, data, isbuy);
        return static_cast<Size>(std::floor(size));
    }
};

/**
 * @brief All-in sizer
 * 
 * Uses all available cash (or a percentage of it) for the position.
 * Can return fractional shares.
 * 
 * Params:
 * - percents: Percentage of cash to use (default: 100)
 */
class AllInSizer : public Sizer {
public:
    struct Params {
        Value percents = 100.0;  // Use 100% by default
    };
    
    AllInSizer() = default;
    explicit AllInSizer(Value percents) { params_.percents = percents; }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief All-in sizer (integer version)
 * 
 * Same as AllInSizer but always returns an integer (truncated) size.
 * 
 * Params:
 * - percents: Percentage of cash to use (default: 100)
 */
class AllInSizerInt : public AllInSizer {
public:
    using AllInSizer::AllInSizer;
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override {
        Value size = AllInSizer::getSizing(comminfo, cash, data, isbuy);
        return static_cast<Size>(std::floor(size));
    }
};

/**
 * @brief Percent reverser sizer
 * 
 * Similar to PercentSizer but doubles the size when reversing a position.
 * 
 * Params:
 * - percents: Percentage of cash to use (default: 20)
 */
class PercentReverser : public Sizer {
public:
    struct Params {
        Value percents = 20.0;
    };
    
    PercentReverser() = default;
    explicit PercentReverser(Value percents) { params_.percents = percents; }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Risk-based sizer (Percent Risk)
 * 
 * Calculates position size based on maximum risk per trade.
 * Risk is defined as the distance from entry to stop-loss multiplied by position size.
 * 
 * Params:
 * - risk: Maximum percentage of portfolio to risk per trade (default: 2)
 * - stoploss: Stop loss distance as percentage of entry price (default: 5)
 */
class RiskSizer : public Sizer {
public:
    struct Params {
        Value risk = 2.0;       // Max risk per trade (% of portfolio)
        Value stoploss = 5.0;   // Stop distance (% of price)
    };
    
    RiskSizer() = default;
    RiskSizer(Value risk, Value stoploss) { 
        params_.risk = risk; 
        params_.stoploss = stoploss; 
    }
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Kelly criterion sizer
 * 
 * Calculates position size using the Kelly criterion formula.
 * Kelly % = W - [(1-W) / R]
 * Where W = win rate, R = win/loss ratio
 * 
 * Params:
 * - winrate: Historical win rate (default: 0.5)
 * - winloss: Win/loss ratio (default: 1.0)
 * - kelly_fraction: Fraction of Kelly to use (default: 0.5 for half-Kelly)
 * - max_percent: Maximum position size as percentage of portfolio (default: 25)
 */
class KellySizer : public Sizer {
public:
    struct Params {
        Value winrate = 0.5;        // Win probability
        Value winloss = 1.0;        // Average win / average loss
        Value kelly_fraction = 0.5; // Use half-Kelly by default
        Value max_percent = 25.0;   // Maximum allocation
    };
    
    KellySizer() = default;
    
    Size getSizing(const CommInfo& comminfo, Value cash,
                   const DataFeed& data, bool isbuy) override;
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

// =============================================================================
// Implementation details (normally in .cpp file)
// =============================================================================

} // namespace bt

// Include implementation details - these would be in a .cpp file in a larger project
#include "bt/broker.hpp"
#include "bt/datafeed.hpp"
#include "bt/strategy.hpp"

namespace bt {

// FixedReverser implementation
inline Size FixedReverser::getSizing(const CommInfo& comminfo, Value cash,
                                     const DataFeed& data, bool isbuy) {
    (void)comminfo; (void)cash;
    
    Size stake = params_.stake;
    
    // Check if we need to reverse (double the stake)
    if (strategy_) {
        Value pos = strategy_->getPosition(const_cast<DataFeed*>(&data));
        // If we're buying and currently short, or selling and currently long
        if ((isbuy && pos < 0) || (!isbuy && pos > 0)) {
            stake *= 2;
        }
    }
    
    return stake;
}

// PercentSizer implementation
inline Size PercentSizer::getSizing(const CommInfo& comminfo, Value cash,
                                    const DataFeed& data, bool isbuy) {
    (void)comminfo; (void)isbuy;
    
    // Get current price
    Value price = data.close()[0];
    if (price <= 0) return 0;
    
    // Calculate cash to use
    Value usecash = cash * (params_.percents / 100.0);
    
    // Calculate size
    Value size = usecash / price;
    
    return static_cast<Size>(std::max(0.0, size));
}

// AllInSizer implementation
inline Size AllInSizer::getSizing(const CommInfo& comminfo, Value cash,
                                  const DataFeed& data, bool isbuy) {
    (void)isbuy;
    
    // Get current price
    Value price = data.close()[0];
    if (price <= 0) return 0;
    
    // Calculate cash to use (percentage of available)
    Value usecash = cash * (params_.percents / 100.0);
    
    // Account for commission if available
    // For now, use simple calculation
    // In full implementation, we'd use comminfo.getsize(price, usecash)
    (void)comminfo;
    
    Value size = usecash / price;
    
    return static_cast<Size>(std::max(0.0, size));
}

// PercentReverser implementation
inline Size PercentReverser::getSizing(const CommInfo& comminfo, Value cash,
                                       const DataFeed& data, bool isbuy) {
    (void)comminfo;
    
    // Get current price
    Value price = data.close()[0];
    if (price <= 0) return 0;
    
    // Calculate base size
    Value usecash = cash * (params_.percents / 100.0);
    Size size = static_cast<Size>(usecash / price);
    
    // Check if we need to reverse (double the stake)
    if (strategy_) {
        Value pos = strategy_->getPosition(const_cast<DataFeed*>(&data));
        if ((isbuy && pos < 0) || (!isbuy && pos > 0)) {
            size *= 2;
        }
    }
    
    return std::max(static_cast<Size>(0), size);
}

// RiskSizer implementation
inline Size RiskSizer::getSizing(const CommInfo& comminfo, Value cash,
                                 const DataFeed& data, bool isbuy) {
    (void)comminfo; (void)isbuy;
    
    // Get current price
    Value price = data.close()[0];
    if (price <= 0) return 0;
    
    // Maximum dollar risk for this trade
    Value maxRiskDollars = cash * (params_.risk / 100.0);
    
    // Stop loss distance in dollars per share
    Value stopDistance = price * (params_.stoploss / 100.0);
    if (stopDistance <= 0) return 0;
    
    // Position size = max risk / stop distance
    Value size = maxRiskDollars / stopDistance;
    
    return static_cast<Size>(std::max(0.0, std::floor(size)));
}

// KellySizer implementation
inline Size KellySizer::getSizing(const CommInfo& comminfo, Value cash,
                                  const DataFeed& data, bool isbuy) {
    (void)comminfo; (void)isbuy;
    
    // Get current price
    Value price = data.close()[0];
    if (price <= 0) return 0;
    
    // Kelly formula: K% = W - (1-W)/R
    // Where W = win rate, R = win/loss ratio
    Value kellyPercent = params_.winrate - ((1.0 - params_.winrate) / params_.winloss);
    
    // Apply Kelly fraction (e.g., half-Kelly)
    kellyPercent *= params_.kelly_fraction;
    
    // Clamp to [0, max_percent]
    kellyPercent = std::max(0.0, std::min(kellyPercent * 100.0, params_.max_percent));
    
    // Calculate cash to use
    Value usecash = cash * (kellyPercent / 100.0);
    
    // Calculate size
    Value size = usecash / price;
    
    return static_cast<Size>(std::max(0.0, std::floor(size)));
}

} // namespace bt
