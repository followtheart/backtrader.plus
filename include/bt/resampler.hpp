/**
 * @file resampler.hpp
 * @brief Data Resampling - TimeFrame conversion
 * 
 * Corresponds to Python's resamplerfilter.py
 * Provides functionality to resample data from one timeframe to another.
 */

#pragma once

#include "bt/timeframe.hpp"
#include "bt/datafeed.hpp"
#include "bt/common.hpp"
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

namespace bt {

/**
 * @brief OHLCV Bar structure for resampling
 */
struct OHLCVBar {
    double datetime = 0.0;
    Value open = 0.0;
    Value high = 0.0;
    Value low = 0.0;
    Value close = 0.0;
    Value volume = 0.0;
    Value openinterest = 0.0;
    
    OHLCVBar() = default;
    
    void reset() {
        datetime = 0.0;
        open = high = low = close = 0.0;
        volume = openinterest = 0.0;
    }
    
    bool isOpen() const {
        return open != 0.0;  // Bar is open if we have data
    }
    
    /**
     * @brief Update bar with new tick data
     */
    void update(double dt, Value o, Value h, Value l, Value c, Value v, Value oi) {
        if (!isOpen()) {
            // First tick - initialize
            datetime = dt;
            open = o;
            high = h;
            low = l;
            close = c;
            volume = v;
            openinterest = oi;
        } else {
            // Update existing bar
            high = std::max(high, h);
            low = std::min(low, l);
            close = c;
            volume += v;
            openinterest = oi;  // Take latest
        }
    }
    
    /**
     * @brief Start a new bar
     */
    void start(double dt, Value o, Value h, Value l, Value c, Value v, Value oi) {
        datetime = dt;
        open = o;
        high = h;
        low = l;
        close = c;
        volume = v;
        openinterest = oi;
    }
};

/**
 * @brief Resampler configuration
 */
struct ResamplerConfig {
    TimeFrame timeframe = TimeFrame::Days;  ///< Target timeframe
    int compression = 1;                     ///< Compression factor
    bool bar2edge = true;                    ///< Use time boundaries
    bool adjbartime = true;                  ///< Adjust bar time
    bool rightedge = true;                   ///< Use right edge of boundary
    int boundoff = 0;                        ///< Boundary offset
    bool takelate = true;                    ///< Take late ticks
    
    ResamplerConfig() = default;
    ResamplerConfig(TimeFrame tf, int comp = 1)
        : timeframe(tf), compression(comp) {}
};

/**
 * @brief Base Resampler class
 * 
 * Resamples data from a smaller timeframe to a larger one.
 * For example: 1-minute bars -> 5-minute bars, or daily -> weekly.
 */
class Resampler {
public:
    Resampler() = default;
    explicit Resampler(const ResamplerConfig& config) : config_(config) {}
    
    /**
     * @brief Set configuration
     */
    void setConfig(const ResamplerConfig& config) { config_ = config; }
    const ResamplerConfig& config() const { return config_; }
    
    /**
     * @brief Process a new bar from source data
     * 
     * @param dt Datetime of bar
     * @param open Open price
     * @param high High price
     * @param low Low price
     * @param close Close price
     * @param volume Volume
     * @param oi Open interest
     * @return true if a resampled bar is complete
     */
    bool process(double dt, Value open, Value high, Value low, 
                 Value close, Value volume, Value oi = 0.0) {
        
        // Check if we need to close current bar
        bool barComplete = false;
        
        if (currentBar_.isOpen()) {
            // Always update current bar with new tick first
            currentBar_.update(dt, open, high, low, close, volume, oi);
            compcount_++;
            
            // Then check if bar is complete
            if (isBarComplete(dt)) {
                // Complete the current bar
                if (config_.adjbartime) {
                    adjustBarTime();
                }
                completedBars_.push_back(currentBar_);
                barComplete = true;
                
                // Reset for next bar
                currentBar_.reset();
                compcount_ = 0;
            }
        } else {
            // Start first bar
            currentBar_.start(dt, open, high, low, close, volume, oi);
            compcount_ = 1;
            
            // Check if single-tick bar should complete immediately
            if (isBarComplete(dt)) {
                if (config_.adjbartime) {
                    adjustBarTime();
                }
                completedBars_.push_back(currentBar_);
                barComplete = true;
                currentBar_.reset();
                compcount_ = 0;
            }
        }
        
        return barComplete;
    }
    
    /**
     * @brief Force close current bar (end of data)
     * @return true if a bar was closed
     */
    bool flush() {
        if (currentBar_.isOpen()) {
            if (config_.adjbartime) {
                adjustBarTime();
            }
            completedBars_.push_back(currentBar_);
            currentBar_.reset();
            return true;
        }
        return false;
    }
    
    /**
     * @brief Get completed bars
     */
    const std::vector<OHLCVBar>& completedBars() const { return completedBars_; }
    
    /**
     * @brief Get and clear completed bars
     */
    std::vector<OHLCVBar> takeCompletedBars() {
        std::vector<OHLCVBar> result = std::move(completedBars_);
        completedBars_.clear();
        return result;
    }
    
    /**
     * @brief Check if there's a pending bar
     */
    bool hasPendingBar() const { return currentBar_.isOpen(); }
    
    /**
     * @brief Get the current pending bar
     */
    const OHLCVBar& pendingBar() const { return currentBar_; }
    
    /**
     * @brief Clear all state
     */
    void reset() {
        currentBar_.reset();
        completedBars_.clear();
        compcount_ = 0;
        lastdt_ = 0;
    }

protected:
    /**
     * @brief Check if current bar is complete
     */
    bool isBarComplete(double newDt) {
        if (!config_.bar2edge) {
            // Simple compression count - bar complete after N ticks
            return (compcount_ >= config_.compression);
        }
        
        // Time boundary based
        switch (config_.timeframe) {
            case TimeFrame::Ticks:
                return compcount_ >= config_.compression;
            case TimeFrame::Seconds:
                return isSecondBarOver(newDt);
            case TimeFrame::Minutes:
                return isMinuteBarOver(newDt);
            case TimeFrame::Days:
                return isDayBarOver(newDt);
            case TimeFrame::Weeks:
                return isWeekBarOver(newDt);
            case TimeFrame::Months:
                return isMonthBarOver(newDt);
            case TimeFrame::Years:
                return isYearBarOver(newDt);
            default:
                return false;
        }
    }
    
    bool isSecondBarOver(double newDt) {
        // Convert datetime to seconds
        int64_t currentSec = static_cast<int64_t>(currentBar_.datetime * 86400);
        int64_t newSec = static_cast<int64_t>(newDt * 86400);
        
        int currentBucket = static_cast<int>(currentSec / config_.compression);
        int newBucket = static_cast<int>(newSec / config_.compression);
        
        return newBucket > currentBucket;
    }
    
    bool isMinuteBarOver(double newDt) {
        // Convert datetime to minutes since midnight
        double currentDayFrac = currentBar_.datetime - std::floor(currentBar_.datetime);
        double newDayFrac = newDt - std::floor(newDt);
        
        int currentMin = static_cast<int>(currentDayFrac * 1440);  // 24*60
        int newMin = static_cast<int>(newDayFrac * 1440);
        
        // Check day boundary
        if (std::floor(newDt) > std::floor(currentBar_.datetime)) {
            return true;
        }
        
        int currentBucket = currentMin / config_.compression;
        int newBucket = newMin / config_.compression;
        
        return newBucket > currentBucket;
    }
    
    bool isDayBarOver(double newDt) {
        int currentDay = static_cast<int>(std::floor(currentBar_.datetime));
        int newDay = static_cast<int>(std::floor(newDt));
        
        if (config_.compression == 1) {
            return newDay > currentDay;
        }
        
        // Multi-day compression
        compcount_++;
        if (newDay > currentDay && compcount_ >= config_.compression) {
            compcount_ = 0;
            return true;
        }
        return false;
    }
    
    bool isWeekBarOver(double newDt) {
        // Week boundaries (Monday-Sunday)
        // Assuming datetime is ordinal day number or similar
        int currentDay = static_cast<int>(std::floor(currentBar_.datetime));
        int newDay = static_cast<int>(std::floor(newDt));
        
        // Simple approximation: week changes every 7 days
        int currentWeek = currentDay / 7;
        int newWeek = newDay / 7;
        
        if (config_.compression == 1) {
            return newWeek > currentWeek;
        }
        
        compcount_++;
        if (newWeek > currentWeek && compcount_ >= config_.compression) {
            compcount_ = 0;
            return true;
        }
        return false;
    }
    
    bool isMonthBarOver(double newDt) {
        // Month boundaries
        int currentDay = static_cast<int>(std::floor(currentBar_.datetime));
        int newDay = static_cast<int>(std::floor(newDt));
        
        // Approximate month (30 days)
        int currentMonth = currentDay / 30;
        int newMonth = newDay / 30;
        
        if (config_.compression == 1) {
            return newMonth > currentMonth;
        }
        
        compcount_++;
        if (newMonth > currentMonth && compcount_ >= config_.compression) {
            compcount_ = 0;
            return true;
        }
        return false;
    }
    
    bool isYearBarOver(double newDt) {
        // Year boundaries
        int currentDay = static_cast<int>(std::floor(currentBar_.datetime));
        int newDay = static_cast<int>(std::floor(newDt));
        
        // Approximate year (365 days)
        int currentYear = currentDay / 365;
        int newYear = newDay / 365;
        
        if (config_.compression == 1) {
            return newYear > currentYear;
        }
        
        compcount_++;
        if (newYear > currentYear && compcount_ >= config_.compression) {
            compcount_ = 0;
            return true;
        }
        return false;
    }
    
    /**
     * @brief Adjust bar time to boundary
     */
    void adjustBarTime() {
        if (!config_.bar2edge || !config_.adjbartime) return;
        
        double dt = currentBar_.datetime;
        
        switch (config_.timeframe) {
            case TimeFrame::Minutes: {
                double dayFrac = dt - std::floor(dt);
                int min = static_cast<int>(dayFrac * 1440);
                int bucket = (min / config_.compression) * config_.compression;
                if (config_.rightedge) {
                    bucket += config_.compression;
                }
                currentBar_.datetime = std::floor(dt) + static_cast<double>(bucket) / 1440.0;
                break;
            }
            case TimeFrame::Days: {
                currentBar_.datetime = std::floor(dt);
                if (config_.rightedge) {
                    currentBar_.datetime += 1.0;
                }
                break;
            }
            default:
                // For other timeframes, keep as-is
                break;
        }
    }

private:
    ResamplerConfig config_;
    OHLCVBar currentBar_;
    std::vector<OHLCVBar> completedBars_;
    int compcount_ = 0;
    double lastdt_ = 0;
};

/**
 * @brief Resampled data feed wrapper
 * 
 * Wraps a source data feed and provides resampled data.
 */
class ResampledDataFeed : public DataFeed {
public:
    ResampledDataFeed(DataFeed* source, const ResamplerConfig& config)
        : source_(source), resampler_(config) {
        setName(source->name() + "_" + 
                std::to_string(config.compression) + 
                timeframe::shortName(config.timeframe));
    }
    
    /**
     * @brief Load data by resampling source
     */
    bool load() override {
        if (!source_ || !source_->load()) return false;
        
        resampler_.reset();
        resampledBars_.clear();
        
        // Process all source bars
        Size srcLen = source_->length();
        for (Size i = 0; i < srcLen; ++i) {
            double dt = source_->datetime()[i];
            Value o = source_->open()[i];
            Value h = source_->high()[i];
            Value l = source_->low()[i];
            Value c = source_->close()[i];
            Value v = source_->volume()[i];
            
            resampler_.process(dt, o, h, l, c, v, 0);
        }
        
        // Flush remaining bar
        resampler_.flush();
        
        // Copy completed bars to our data lines
        auto bars = resampler_.takeCompletedBars();
        for (const auto& bar : bars) {
            resampledBars_.push_back(bar);
            datetime().push(bar.datetime);
            open().push(bar.open);
            high().push(bar.high);
            low().push(bar.low);
            close().push(bar.close);
            volume().push(bar.volume);
        }
        
        return !resampledBars_.empty();
    }
    
    /**
     * @brief Get source data feed
     */
    DataFeed* source() const { return source_; }
    
    /**
     * @brief Get resampler configuration
     */
    const ResamplerConfig& resamplerConfig() const { return resampler_.config(); }

private:
    DataFeed* source_ = nullptr;
    Resampler resampler_;
    std::vector<OHLCVBar> resampledBars_;
};

/**
 * @brief Helper to create a resampled data feed
 */
inline std::shared_ptr<ResampledDataFeed> resampleData(
    DataFeed* source,
    TimeFrame timeframe,
    int compression = 1,
    bool bar2edge = true,
    bool adjbartime = true,
    bool rightedge = true) {
    
    ResamplerConfig config;
    config.timeframe = timeframe;
    config.compression = compression;
    config.bar2edge = bar2edge;
    config.adjbartime = adjbartime;
    config.rightedge = rightedge;
    
    return std::make_shared<ResampledDataFeed>(source, config);
}

} // namespace bt
