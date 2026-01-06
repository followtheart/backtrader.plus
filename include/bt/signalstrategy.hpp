/**
 * @file signalstrategy.hpp
 * @brief Signal-based automatic strategy
 * 
 * Corresponds to Python's signals/strategy pattern
 * Provides automatic trading based on registered signals.
 */

#pragma once

#include "bt/strategy.hpp"
#include "bt/signal.hpp"
#include "bt/params.hpp"
#include <map>

namespace bt {

/**
 * @brief Signal accumulation mode
 */
enum class SignalAccumMode {
    LongShort,  ///< Allow both long and short positions
    LongOnly,   ///< Only allow long positions
    ShortOnly   ///< Only allow short positions
};

/**
 * @brief Signal-based Strategy
 * 
 * This strategy automatically executes trades based on registered signals.
 * It handles signal aggregation, position sizing, and order execution.
 * 
 * Usage:
 * @code
 * // Create signal strategy
 * SignalStrategy strategy;
 * strategy.setSignalAccumMode(SignalAccumMode::LongShort);
 * 
 * // Add signals
 * auto crossoverSignal = new Signal(smaCrossover, SignalType::SIGNAL_LONGSHORT);
 * strategy.addSignal(crossoverSignal, SignalType::SIGNAL_LONGSHORT);
 * 
 * // Run with cerebro
 * cerebro.addStrategy(&strategy);
 * @endcode
 */
class SignalStrategy : public Strategy {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(signal_accumulate, false)   // Accumulate signals or not
        BT_PARAM(signal_concurrent, false)   // Allow concurrent signals
        BT_PARAM(signal_percents, false)     // Use percentages for sizing
        BT_PARAM(signal_stake, 1)            // Fixed stake size
    BT_PARAMS_END()

    SignalStrategy() = default;
    virtual ~SignalStrategy() = default;
    
    // ==================== Configuration ====================
    
    /**
     * @brief Set signal accumulation mode
     */
    void setSignalAccumMode(SignalAccumMode mode) { accumMode_ = mode; }
    SignalAccumMode signalAccumMode() const { return accumMode_; }
    
    /**
     * @brief Set whether to exit on opposing signal
     */
    void setExitOnOpposite(bool exit) { exitOnOpposite_ = exit; }
    bool exitOnOpposite() const { return exitOnOpposite_; }
    
    /**
     * @brief Set whether to use all signals or first matching
     */
    void setUseAllSignals(bool all) { useAllSignals_ = all; }
    bool useAllSignals() const { return useAllSignals_; }
    
    // ==================== Signal Registration ====================
    
    /**
     * @brief Add a long entry signal
     */
    void addLongSignal(Signal* signal, Size dataIndex = 0) {
        signals_.addSignal(signal, SignalType::SIGNAL_LONG, dataIndex);
    }
    
    /**
     * @brief Add a short entry signal
     */
    void addShortSignal(Signal* signal, Size dataIndex = 0) {
        signals_.addSignal(signal, SignalType::SIGNAL_SHORT, dataIndex);
    }
    
    /**
     * @brief Add a long/short reversing signal
     */
    void addLongShortSignal(Signal* signal, Size dataIndex = 0) {
        signals_.addSignal(signal, SignalType::SIGNAL_LONGSHORT, dataIndex);
    }
    
    /**
     * @brief Add a long exit signal
     */
    void addLongExitSignal(Signal* signal, Size dataIndex = 0) {
        signals_.addSignal(signal, SignalType::SIGNAL_LONGEXIT, dataIndex);
    }
    
    /**
     * @brief Add a short exit signal
     */
    void addShortExitSignal(Signal* signal, Size dataIndex = 0) {
        signals_.addSignal(signal, SignalType::SIGNAL_SHORTEXIT, dataIndex);
    }
    
    // ==================== Lifecycle ====================
    
    void init() override {
        // Subclasses can override to add indicators
    }
    
    void next() override {
        processSignals();
    }

protected:
    /**
     * @brief Process all signals and execute trades
     */
    virtual void processSignals() {
        // Get current position
        Value pos = position();
        
        // Check for exit signals first
        if (pos > 0 && signals_.hasLongExit()) {
            // Exit long position
            closePosition();
            pos = 0;
        } else if (pos < 0 && signals_.hasShortExit()) {
            // Exit short position
            closePosition();
            pos = 0;
        }
        
        // Check accumulation mode restrictions
        bool canGoLong = (accumMode_ != SignalAccumMode::ShortOnly);
        bool canGoShort = (accumMode_ != SignalAccumMode::LongOnly);
        
        // Check for entry signals
        bool longSignal = canGoLong && signals_.hasLongEntry();
        bool shortSignal = canGoShort && signals_.hasShortEntry();
        
        // Handle conflicting signals
        if (longSignal && shortSignal) {
            // Both signals present - depends on current position and settings
            if (pos > 0) {
                longSignal = false;  // Already long, ignore long signal
            } else if (pos < 0) {
                shortSignal = false;  // Already short, ignore short signal
            } else {
                // Flat - take the first one or cancel both
                if (!useAllSignals_) {
                    shortSignal = false;  // Default to long
                }
            }
        }
        
        // Execute signals
        if (longSignal) {
            if (pos < 0 && exitOnOpposite_) {
                // Close short first
                closePosition();
            }
            if (pos <= 0) {
                // Enter long
                Size size = getSizing(nullptr, true);
                if (size > 0) {
                    buy(nullptr, size);
                }
            }
        }
        
        if (shortSignal) {
            if (pos > 0 && exitOnOpposite_) {
                // Close long first
                closePosition();
            }
            if (pos >= 0) {
                // Enter short
                Size size = getSizing(nullptr, false);
                if (size > 0) {
                    sell(nullptr, size);
                }
            }
        }
    }
    
    /**
     * @brief Get the signal strength (for sizing)
     */
    virtual Value getSignalStrength() const {
        // Default: return 1.0
        // Override to implement custom signal strength calculation
        return 1.0;
    }

private:
    SignalAccumMode accumMode_ = SignalAccumMode::LongShort;
    bool exitOnOpposite_ = true;
    bool useAllSignals_ = false;
};

/**
 * @brief Simple crossover signal strategy
 * 
 * Convenience class for common crossover-based strategies.
 */
class CrossoverSignalStrategy : public SignalStrategy {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(fast_period, 10)
        BT_PARAM(slow_period, 30)
    BT_PARAMS_END()
    
    CrossoverSignalStrategy() = default;
    
    /**
     * @brief Set the crossover signal
     * 
     * @param fastLine Fast indicator line
     * @param slowLine Slow indicator line
     * @param signalType How to interpret the crossover
     */
    void setCrossover(LineBuffer* fastLine, LineBuffer* slowLine,
                      SignalType signalType = SignalType::SIGNAL_LONGSHORT) {
        fastLine_ = fastLine;
        slowLine_ = slowLine;
        crossoverType_ = signalType;
    }
    
    void next() override {
        // Check for crossover
        if (!fastLine_ || !slowLine_) {
            SignalStrategy::next();
            return;
        }
        
        Value fastCurrent = (*fastLine_)[0];
        Value fastPrev = (*fastLine_)[1];
        Value slowCurrent = (*slowLine_)[0];
        Value slowPrev = (*slowLine_)[1];
        
        bool crossUp = (fastPrev <= slowPrev) && (fastCurrent > slowCurrent);
        bool crossDown = (fastPrev >= slowPrev) && (fastCurrent < slowCurrent);
        
        Value pos = position();
        
        if (crossUp) {
            if (pos <= 0) {
                if (pos < 0) closePosition();
                buy();
            }
        } else if (crossDown) {
            if (pos >= 0) {
                if (pos > 0) closePosition();
                if (signalAccumMode() != SignalAccumMode::LongOnly) {
                    sell();
                }
            }
        }
    }

private:
    LineBuffer* fastLine_ = nullptr;
    LineBuffer* slowLine_ = nullptr;
    SignalType crossoverType_ = SignalType::SIGNAL_LONGSHORT;
};

} // namespace bt
