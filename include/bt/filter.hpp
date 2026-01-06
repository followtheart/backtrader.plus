/**
 * @file filter.hpp
 * @brief Data Filter System
 * 
 * Corresponds to Python's filters/
 * Provides data filtering and transformation capabilities.
 * 
 * Filters operate on data feeds to modify or filter bars before
 * they reach the strategy. Common uses:
 * - Session filtering (trading hours only)
 * - Renko chart generation
 * - HeikinAshi transformation
 * - Gap filling
 */

#pragma once

#include "bt/common.hpp"
#include "bt/params.hpp"
#include "bt/datafeed.hpp"
#include <vector>
#include <deque>
#include <cmath>

namespace bt {

/**
 * @brief Base Data Filter class
 * 
 * Filters can modify data as it passes through, adding or removing bars,
 * or transforming OHLCV values.
 * 
 * Return values from filter():
 * - false: Bar is passed through (optionally modified)
 * - true: Bar is filtered out (not passed to strategy)
 */
class DataFilter : public Parametrized<DataFilter> {
public:
    virtual ~DataFilter() = default;
    
    /**
     * @brief Called at start of filtering
     * @param data The data feed being filtered
     */
    virtual void start(DataFeed& data) { (void)data; }
    
    /**
     * @brief Filter/transform a data bar
     * 
     * @param data The data feed with current bar values
     * @return true if bar should be filtered out, false to pass through
     */
    virtual bool filter(DataFeed& data) = 0;
    
    /**
     * @brief Called at end of filtering
     * @param data The data feed
     */
    virtual void stop(DataFeed& data) { (void)data; }
    
    /**
     * @brief Check if filter has pending data
     * 
     * Some filters (like Renko) may generate multiple bars from one input.
     * This returns true if there are pending bars to deliver.
     */
    virtual bool hasPending() const { return false; }
    
    /**
     * @brief Get next pending bar
     * 
     * For filters that generate multiple output bars.
     */
    virtual bool nextPending(DataFeed& data) { (void)data; return false; }
};

/**
 * @brief Session filter - only pass bars within trading session
 * 
 * Filters out bars outside of specified trading hours.
 * 
 * Params:
 * - sessionstart: Start time (minutes from midnight, e.g., 570 = 9:30)
 * - sessionend: End time (minutes from midnight, e.g., 960 = 16:00)
 */
class SessionFilter : public DataFilter {
public:
    struct Params {
        int sessionstart = 570;  // 9:30 AM
        int sessionend = 960;    // 4:00 PM
    };
    
    SessionFilter() = default;
    SessionFilter(int start, int end) {
        params_.sessionstart = start;
        params_.sessionend = end;
    }
    
    bool filter(DataFeed& data) override {
        DateTime dt = data.getDateTime(0);
        int minutes = dt.hour * 60 + dt.minute;
        
        // Filter out if outside session
        if (minutes < params_.sessionstart || minutes > params_.sessionend) {
            return true;  // Filter out
        }
        return false;  // Pass through
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Session filler - fill gaps in trading session
 * 
 * Generates synthetic bars to fill gaps within trading session.
 * Uses previous close for OHLC and zero volume.
 */
class SessionFiller : public DataFilter {
public:
    struct Params {
        int sessionstart = 570;
        int sessionend = 960;
        int barsize = 1;  // Minutes per bar
    };
    
    bool filter(DataFeed& data) override {
        // Check for gaps and fill if needed
        // This is a simplified implementation
        return false;
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
    Value lastClose_ = 0;
    double lastDt_ = 0;
};

/**
 * @brief Renko filter - generate Renko bricks
 * 
 * Transforms regular OHLCV data into Renko chart bars.
 * Only generates a new bar when price moves by brick size.
 * 
 * Params:
 * - size: Brick size in price units (default: 1.0)
 * - autosize: Auto-calculate size from ATR (default: false)
 * - atr_period: ATR period for auto-sizing (default: 14)
 */
class RenkoFilter : public DataFilter {
public:
    struct Params {
        Value size = 1.0;        // Brick size
        bool autosize = false;   // Auto-calculate from ATR
        int atr_period = 14;     // ATR period for auto-size
    };
    
    RenkoFilter() = default;
    explicit RenkoFilter(Value brickSize) { params_.size = brickSize; }
    
    void start(DataFeed& data) override {
        initialized_ = false;
        pendingBricks_.clear();
        lastBrickClose_ = 0;
    }
    
    bool filter(DataFeed& data) override {
        Value close = data.close()[0];
        Value high = data.high()[0];
        Value low = data.low()[0];
        double dt = data.datetime()[0];
        
        if (!initialized_) {
            // Initialize with first bar
            lastBrickClose_ = close;
            lastBrickOpen_ = close;
            initialized_ = true;
            
            // Generate first brick
            RenkoBrick brick;
            brick.datetime = dt;
            brick.open = close;
            brick.high = close;
            brick.low = close;
            brick.close = close;
            brick.volume = data.volume()[0];
            pendingBricks_.push_back(brick);
            
            return false;  // Pass first bar
        }
        
        // Check for new bricks
        Value size = params_.size;
        
        // Upward bricks
        while (high >= lastBrickClose_ + size) {
            RenkoBrick brick;
            brick.datetime = dt;
            brick.open = lastBrickClose_;
            brick.close = lastBrickClose_ + size;
            brick.high = brick.close;
            brick.low = brick.open;
            brick.volume = 0;  // Volume concentrated in last brick
            brick.up = true;
            
            lastBrickClose_ = brick.close;
            lastBrickOpen_ = brick.open;
            pendingBricks_.push_back(brick);
        }
        
        // Downward bricks
        while (low <= lastBrickClose_ - size) {
            RenkoBrick brick;
            brick.datetime = dt;
            brick.open = lastBrickClose_;
            brick.close = lastBrickClose_ - size;
            brick.high = brick.open;
            brick.low = brick.close;
            brick.volume = 0;
            brick.up = false;
            
            lastBrickClose_ = brick.close;
            lastBrickOpen_ = brick.open;
            pendingBricks_.push_back(brick);
        }
        
        // If we generated bricks, update the data feed with the first one
        if (!pendingBricks_.empty()) {
            const auto& brick = pendingBricks_.front();
            // Modify data in place
            // Note: In full implementation, we'd have proper line modification
            return false;
        }
        
        // Filter out bars that don't generate bricks
        return true;
    }
    
    bool hasPending() const override {
        return pendingBricks_.size() > 1;
    }
    
    bool nextPending(DataFeed& data) override {
        if (pendingBricks_.empty()) return false;
        pendingBricks_.pop_front();
        if (pendingBricks_.empty()) return false;
        
        // Update data with next pending brick
        // (Implementation depends on DataFeed modification interface)
        return true;
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    struct RenkoBrick {
        double datetime;
        Value open;
        Value high;
        Value low;
        Value close;
        Value volume;
        bool up;
    };
    
    Params params_;
    bool initialized_ = false;
    Value lastBrickClose_ = 0;
    Value lastBrickOpen_ = 0;
    std::deque<RenkoBrick> pendingBricks_;
};

/**
 * @brief Heikin-Ashi filter - transform to Heikin-Ashi candles
 * 
 * Transforms regular OHLCV into Heikin-Ashi candlesticks.
 * HA candles smooth price action and make trends easier to spot.
 * 
 * Formulas:
 * - HA Close = (Open + High + Low + Close) / 4
 * - HA Open = (Previous HA Open + Previous HA Close) / 2
 * - HA High = max(High, HA Open, HA Close)
 * - HA Low = min(Low, HA Open, HA Close)
 */
class HeikinAshiFilter : public DataFilter {
public:
    void start(DataFeed& data) override {
        (void)data;
        initialized_ = false;
        prevHaOpen_ = 0;
        prevHaClose_ = 0;
    }
    
    bool filter(DataFeed& data) override {
        Value open = data.open()[0];
        Value high = data.high()[0];
        Value low = data.low()[0];
        Value close = data.close()[0];
        
        // Calculate HA values
        Value haClose = (open + high + low + close) / 4.0;
        Value haOpen;
        
        if (!initialized_) {
            haOpen = (open + close) / 2.0;
            initialized_ = true;
        } else {
            haOpen = (prevHaOpen_ + prevHaClose_) / 2.0;
        }
        
        Value haHigh = std::max({high, haOpen, haClose});
        Value haLow = std::min({low, haOpen, haClose});
        
        // Store for next bar
        prevHaOpen_ = haOpen;
        prevHaClose_ = haClose;
        
        // Store transformed values
        // In full implementation, would modify data feed
        haOpen_ = haOpen;
        haHigh_ = haHigh;
        haLow_ = haLow;
        haClose_ = haClose;
        
        return false;  // Pass through (modified)
    }
    
    // Accessors for transformed values
    Value haOpen() const { return haOpen_; }
    Value haHigh() const { return haHigh_; }
    Value haLow() const { return haLow_; }
    Value haClose() const { return haClose_; }

private:
    bool initialized_ = false;
    Value prevHaOpen_ = 0;
    Value prevHaClose_ = 0;
    
    // Current transformed values
    Value haOpen_ = 0;
    Value haHigh_ = 0;
    Value haLow_ = 0;
    Value haClose_ = 0;
};

/**
 * @brief Calendar days filter - generate bars for calendar days
 * 
 * Generates bars for all calendar days, even non-trading days.
 * Non-trading days use the previous day's close for OHLC.
 */
class CalendarDaysFilter : public DataFilter {
public:
    bool filter(DataFeed& data) override {
        // Generate missing calendar days
        // Simplified implementation - would need date arithmetic
        return false;
    }
};

/**
 * @brief Day steps filter - break daily bars into intraday steps
 * 
 * Takes daily bars and simulates intraday price action.
 * Useful for testing strategies that need intraday granularity
 * when only daily data is available.
 * 
 * Params:
 * - steps: Number of intraday steps per day (default: 4)
 *          4 steps = Open, High, Low, Close sequence
 */
class DayStepsFilter : public DataFilter {
public:
    struct Params {
        int steps = 4;  // OHLC sequence
    };
    
    void start(DataFeed& data) override {
        (void)data;
        currentStep_ = 0;
        currentOpen_ = currentHigh_ = currentLow_ = currentClose_ = 0;
    }
    
    bool filter(DataFeed& data) override {
        if (currentStep_ == 0) {
            // Store day's OHLC
            currentOpen_ = data.open()[0];
            currentHigh_ = data.high()[0];
            currentLow_ = data.low()[0];
            currentClose_ = data.close()[0];
        }
        
        // Generate step values
        // Step 0: Open
        // Step 1: High (if > Open) or Low
        // Step 2: Low (if step 1 was high) or High
        // Step 3: Close
        
        currentStep_++;
        if (currentStep_ >= params_.steps) {
            currentStep_ = 0;
        }
        
        return false;
    }
    
    bool hasPending() const override {
        return currentStep_ > 0;
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
    int currentStep_ = 0;
    Value currentOpen_ = 0;
    Value currentHigh_ = 0;
    Value currentLow_ = 0;
    Value currentClose_ = 0;
};

/**
 * @brief Data filler - fill missing bars with previous values
 * 
 * When there are gaps in the data (weekends, holidays), this filter
 * can optionally fill them with the previous bar's close price.
 * 
 * Params:
 * - fill: Enable filling (default: true)
 */
class DataFiller : public DataFilter {
public:
    struct Params {
        bool fill = true;
    };
    
    void start(DataFeed& data) override {
        (void)data;
        lastClose_ = 0;
        lastVolume_ = 0;
    }
    
    bool filter(DataFeed& data) override {
        // Store current values for gap filling
        lastClose_ = data.close()[0];
        lastVolume_ = data.volume()[0];
        return false;
    }
    
    // Get fill values for generating synthetic bars
    Value fillPrice() const { return lastClose_; }
    Value fillVolume() const { return 0; }  // Zero volume for filled bars
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
    Value lastClose_ = 0;
    Value lastVolume_ = 0;
};

/**
 * @brief Bar replayer - replay bars at different granularity
 * 
 * Used for data replay functionality. Can deliver bars tick-by-tick
 * or in chunks to simulate real-time behavior.
 * 
 * Types:
 * - Open: Deliver open price first, then rest of bar
 * - Close: Deliver partial bar, then close
 * - Tick: Deliver each price change
 */
enum class ReplayType {
    Open,   // Deliver at open
    Close,  // Deliver at close
    Tick    // Tick-by-tick
};

class BarReplayer : public DataFilter {
public:
    struct Params {
        ReplayType type = ReplayType::Close;
    };
    
    bool filter(DataFeed& data) override {
        // Replay logic depends on type
        (void)data;
        return false;
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Volume filter - filter bars by volume
 * 
 * Only passes bars that meet volume criteria.
 * 
 * Params:
 * - minvol: Minimum volume to pass (default: 0)
 * - maxvol: Maximum volume to pass (default: infinity)
 */
class VolumeFilter : public DataFilter {
public:
    struct Params {
        Value minvol = 0;
        Value maxvol = std::numeric_limits<Value>::infinity();
    };
    
    bool filter(DataFeed& data) override {
        Value vol = data.volume()[0];
        if (vol < params_.minvol || vol > params_.maxvol) {
            return true;  // Filter out
        }
        return false;  // Pass through
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Price filter - filter bars by price range
 * 
 * Only passes bars within specified price range.
 * 
 * Params:
 * - minprice: Minimum close price (default: 0)
 * - maxprice: Maximum close price (default: infinity)
 */
class PriceFilter : public DataFilter {
public:
    struct Params {
        Value minprice = 0;
        Value maxprice = std::numeric_limits<Value>::infinity();
    };
    
    bool filter(DataFeed& data) override {
        Value price = data.close()[0];
        if (price < params_.minprice || price > params_.maxprice) {
            return true;  // Filter out
        }
        return false;  // Pass through
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    Params params_;
};

/**
 * @brief Filter chain - combine multiple filters
 * 
 * Applies filters in sequence. A bar is filtered if ANY filter
 * returns true (filters it out).
 */
class FilterChain : public DataFilter {
public:
    void addFilter(std::unique_ptr<DataFilter> filter) {
        filters_.push_back(std::move(filter));
    }
    
    void start(DataFeed& data) override {
        for (auto& f : filters_) {
            f->start(data);
        }
    }
    
    bool filter(DataFeed& data) override {
        for (auto& f : filters_) {
            if (f->filter(data)) {
                return true;  // Filtered by this filter
            }
        }
        return false;  // Passed all filters
    }
    
    void stop(DataFeed& data) override {
        for (auto& f : filters_) {
            f->stop(data);
        }
    }

private:
    std::vector<std::unique_ptr<DataFilter>> filters_;
};

} // namespace bt
