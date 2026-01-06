/**
 * @file signal.hpp
 * @brief Signal System for automatic trading signals
 * 
 * Corresponds to Python's signal.py
 * Provides signal types and Signal indicator class for 
 * strategy signal management.
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/common.hpp"
#include <string>
#include <vector>

namespace bt {

/**
 * @brief Signal types enumeration
 * 
 * These define the different types of trading signals that can be generated.
 * Matches Python backtrader's signal types exactly.
 */
enum class SignalType : int {
    SIGNAL_NONE = 0,          ///< No signal
    SIGNAL_LONGSHORT = 1,     ///< Long/Short reversing signal (positive = long, negative = short)
    SIGNAL_LONG = 2,          ///< Long entry signal (positive = enter long)
    SIGNAL_LONG_INV = 3,      ///< Long entry inverted (negative = enter long)
    SIGNAL_LONG_ANY = 4,      ///< Long entry any (any value = enter long)
    SIGNAL_SHORT = 5,         ///< Short entry signal (negative = enter short)
    SIGNAL_SHORT_INV = 6,     ///< Short entry inverted (positive = enter short)
    SIGNAL_SHORT_ANY = 7,     ///< Short entry any (any value = enter short)
    SIGNAL_LONGEXIT = 8,      ///< Long exit signal (negative = exit long)
    SIGNAL_LONGEXIT_INV = 9,  ///< Long exit inverted (positive = exit long)
    SIGNAL_LONGEXIT_ANY = 10, ///< Long exit any (any value = exit long)
    SIGNAL_SHORTEXIT = 11,    ///< Short exit signal (positive = exit short)
    SIGNAL_SHORTEXIT_INV = 12,///< Short exit inverted (negative = exit short)
    SIGNAL_SHORTEXIT_ANY = 13 ///< Short exit any (any value = exit short)
};

/**
 * @brief Array of all signal types for iteration
 */
constexpr SignalType AllSignalTypes[] = {
    SignalType::SIGNAL_NONE,
    SignalType::SIGNAL_LONGSHORT,
    SignalType::SIGNAL_LONG,
    SignalType::SIGNAL_LONG_INV,
    SignalType::SIGNAL_LONG_ANY,
    SignalType::SIGNAL_SHORT,
    SignalType::SIGNAL_SHORT_INV,
    SignalType::SIGNAL_SHORT_ANY,
    SignalType::SIGNAL_LONGEXIT,
    SignalType::SIGNAL_LONGEXIT_INV,
    SignalType::SIGNAL_LONGEXIT_ANY,
    SignalType::SIGNAL_SHORTEXIT,
    SignalType::SIGNAL_SHORTEXIT_INV,
    SignalType::SIGNAL_SHORTEXIT_ANY
};

constexpr size_t NumSignalTypes = sizeof(AllSignalTypes) / sizeof(AllSignalTypes[0]);

/**
 * @brief Signal type utilities
 */
namespace signal_utils {

/**
 * @brief Get the name of a signal type
 */
inline std::string name(SignalType st) {
    switch (st) {
        case SignalType::SIGNAL_NONE: return "SIGNAL_NONE";
        case SignalType::SIGNAL_LONGSHORT: return "SIGNAL_LONGSHORT";
        case SignalType::SIGNAL_LONG: return "SIGNAL_LONG";
        case SignalType::SIGNAL_LONG_INV: return "SIGNAL_LONG_INV";
        case SignalType::SIGNAL_LONG_ANY: return "SIGNAL_LONG_ANY";
        case SignalType::SIGNAL_SHORT: return "SIGNAL_SHORT";
        case SignalType::SIGNAL_SHORT_INV: return "SIGNAL_SHORT_INV";
        case SignalType::SIGNAL_SHORT_ANY: return "SIGNAL_SHORT_ANY";
        case SignalType::SIGNAL_LONGEXIT: return "SIGNAL_LONGEXIT";
        case SignalType::SIGNAL_LONGEXIT_INV: return "SIGNAL_LONGEXIT_INV";
        case SignalType::SIGNAL_LONGEXIT_ANY: return "SIGNAL_LONGEXIT_ANY";
        case SignalType::SIGNAL_SHORTEXIT: return "SIGNAL_SHORTEXIT";
        case SignalType::SIGNAL_SHORTEXIT_INV: return "SIGNAL_SHORTEXIT_INV";
        case SignalType::SIGNAL_SHORTEXIT_ANY: return "SIGNAL_SHORTEXIT_ANY";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Check if signal type is a long entry signal
 */
inline bool isLongEntry(SignalType st) {
    return st == SignalType::SIGNAL_LONGSHORT ||
           st == SignalType::SIGNAL_LONG ||
           st == SignalType::SIGNAL_LONG_INV ||
           st == SignalType::SIGNAL_LONG_ANY;
}

/**
 * @brief Check if signal type is a short entry signal
 */
inline bool isShortEntry(SignalType st) {
    return st == SignalType::SIGNAL_LONGSHORT ||
           st == SignalType::SIGNAL_SHORT ||
           st == SignalType::SIGNAL_SHORT_INV ||
           st == SignalType::SIGNAL_SHORT_ANY;
}

/**
 * @brief Check if signal type is a long exit signal
 */
inline bool isLongExit(SignalType st) {
    return st == SignalType::SIGNAL_LONGEXIT ||
           st == SignalType::SIGNAL_LONGEXIT_INV ||
           st == SignalType::SIGNAL_LONGEXIT_ANY;
}

/**
 * @brief Check if signal type is a short exit signal
 */
inline bool isShortExit(SignalType st) {
    return st == SignalType::SIGNAL_SHORTEXIT ||
           st == SignalType::SIGNAL_SHORTEXIT_INV ||
           st == SignalType::SIGNAL_SHORTEXIT_ANY;
}

/**
 * @brief Check if signal type is an exit signal
 */
inline bool isExit(SignalType st) {
    return isLongExit(st) || isShortExit(st);
}

/**
 * @brief Check if signal type is an entry signal  
 */
inline bool isEntry(SignalType st) {
    return isLongEntry(st) || isShortEntry(st);
}

/**
 * @brief Evaluate if a value triggers a signal based on signal type
 * 
 * @param value The signal value
 * @param st The signal type
 * @return 1 for long signal, -1 for short signal, 0 for no signal
 */
inline int evaluate(Value value, SignalType st) {
    if (value == 0.0) {
        return 0;  // Zero value never triggers (except for ANY types)
    }
    
    switch (st) {
        case SignalType::SIGNAL_NONE:
            return 0;
            
        case SignalType::SIGNAL_LONGSHORT:
            return (value > 0) ? 1 : ((value < 0) ? -1 : 0);
            
        case SignalType::SIGNAL_LONG:
            return (value > 0) ? 1 : 0;
            
        case SignalType::SIGNAL_LONG_INV:
            return (value < 0) ? 1 : 0;
            
        case SignalType::SIGNAL_LONG_ANY:
            return (value != 0) ? 1 : 0;
            
        case SignalType::SIGNAL_SHORT:
            return (value < 0) ? -1 : 0;
            
        case SignalType::SIGNAL_SHORT_INV:
            return (value > 0) ? -1 : 0;
            
        case SignalType::SIGNAL_SHORT_ANY:
            return (value != 0) ? -1 : 0;
            
        case SignalType::SIGNAL_LONGEXIT:
            return (value < 0) ? 1 : 0;  // Exit long
            
        case SignalType::SIGNAL_LONGEXIT_INV:
            return (value > 0) ? 1 : 0;  // Exit long
            
        case SignalType::SIGNAL_LONGEXIT_ANY:
            return (value != 0) ? 1 : 0;  // Exit long
            
        case SignalType::SIGNAL_SHORTEXIT:
            return (value > 0) ? -1 : 0;  // Exit short
            
        case SignalType::SIGNAL_SHORTEXIT_INV:
            return (value < 0) ? -1 : 0;  // Exit short
            
        case SignalType::SIGNAL_SHORTEXIT_ANY:
            return (value != 0) ? -1 : 0;  // Exit short
            
        default:
            return 0;
    }
}

} // namespace signal_utils

/**
 * @brief Signal indicator class
 * 
 * A Signal wraps another indicator or data line and interprets 
 * its values as trading signals based on the assigned signal type.
 * 
 * Usage:
 * @code
 * // Create a signal from an SMA crossover
 * auto smaFast = SMA(data, 10);
 * auto smaSlow = SMA(data, 30);
 * Signal signal(smaFast - smaSlow, SignalType::SIGNAL_LONGSHORT);
 * @endcode
 */
class Signal : public Indicator {
public:
    Signal() {
        addLine("signal");
    }
    
    /**
     * @brief Construct from a line buffer
     */
    explicit Signal(LineBuffer* source, SignalType type = SignalType::SIGNAL_NONE)
        : source_(source), signalType_(type) {
        addLine("signal");
    }
    
    /**
     * @brief Construct from another indicator
     */
    explicit Signal(Indicator* ind, SignalType type = SignalType::SIGNAL_NONE)
        : source_(ind ? &ind->lines0() : nullptr)
        , signalType_(type)
        , sourceIndicator_(ind) {
        addLine("signal");
    }
    
    /**
     * @brief Set the signal type
     */
    void setSignalType(SignalType type) { signalType_ = type; }
    
    /**
     * @brief Get the signal type
     */
    SignalType signalType() const { return signalType_; }
    
    /**
     * @brief Get the signal type name
     */
    std::string signalTypeName() const {
        return signal_utils::name(signalType_);
    }
    
    /**
     * @brief Set source line
     */
    void setSource(LineBuffer* source) { source_ = source; }
    
    /**
     * @brief Called for each bar to calculate signal value
     */
    void next() override {
        if (!source_) return;
        
        // Copy source value to signal line
        Value srcValue = (*source_)[0];
        lines0().push(srcValue);
    }
    
    /**
     * @brief Get the current signal value
     */
    Value value(int offset = 0) const {
        if (!source_) return 0.0;
        return (*source_)[offset];
    }
    
    /**
     * @brief Evaluate the current signal
     * @return 1 for long, -1 for short, 0 for no signal
     */
    int evaluate(int offset = 0) const {
        return signal_utils::evaluate(value(offset), signalType_);
    }
    
    /**
     * @brief Check if current signal indicates long entry
     */
    bool isLong(int offset = 0) const {
        if (!signal_utils::isLongEntry(signalType_)) return false;
        return evaluate(offset) > 0;
    }
    
    /**
     * @brief Check if current signal indicates short entry
     */
    bool isShort(int offset = 0) const {
        if (!signal_utils::isShortEntry(signalType_)) return false;
        return evaluate(offset) < 0;
    }
    
    /**
     * @brief Check if current signal indicates long exit
     */
    bool isLongExit(int offset = 0) const {
        if (!signal_utils::isLongExit(signalType_)) return false;
        return evaluate(offset) != 0;
    }
    
    /**
     * @brief Check if current signal indicates short exit
     */
    bool isShortExit(int offset = 0) const {
        if (!signal_utils::isShortExit(signalType_)) return false;
        return evaluate(offset) != 0;
    }

protected:
    LineBuffer* source_ = nullptr;
    SignalType signalType_ = SignalType::SIGNAL_NONE;
    Indicator* sourceIndicator_ = nullptr;  // Keep reference if constructed from indicator
};

/**
 * @brief Signal configuration for a data feed
 * 
 * Used by SignalStrategy to manage signals for different data feeds.
 */
struct SignalConfig {
    Signal* signal = nullptr;
    SignalType signalType = SignalType::SIGNAL_NONE;
    Size dataIndex = 0;
    
    SignalConfig() = default;
    SignalConfig(Signal* sig, SignalType type, Size idx = 0)
        : signal(sig), signalType(type), dataIndex(idx) {}
};

/**
 * @brief Collection of signals grouped by type
 */
class SignalGroup {
public:
    /**
     * @brief Add a signal
     */
    void addSignal(Signal* signal, SignalType type, Size dataIndex = 0) {
        signals_.emplace_back(signal, type, dataIndex);
        
        // Categorize
        if (signal_utils::isLongEntry(type) && !signal_utils::isShortEntry(type)) {
            longEntrySignals_.push_back(signals_.size() - 1);
        }
        if (signal_utils::isShortEntry(type) && !signal_utils::isLongEntry(type)) {
            shortEntrySignals_.push_back(signals_.size() - 1);
        }
        if (type == SignalType::SIGNAL_LONGSHORT) {
            longShortSignals_.push_back(signals_.size() - 1);
        }
        if (signal_utils::isLongExit(type)) {
            longExitSignals_.push_back(signals_.size() - 1);
        }
        if (signal_utils::isShortExit(type)) {
            shortExitSignals_.push_back(signals_.size() - 1);
        }
    }
    
    /**
     * @brief Get all signals
     */
    const std::vector<SignalConfig>& signals() const { return signals_; }
    
    /**
     * @brief Check if there's a long entry signal
     */
    bool hasLongEntry() const {
        // Check long/short signals first
        for (size_t idx : longShortSignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() > 0) {
                return true;
            }
        }
        // Then check dedicated long entry signals
        for (size_t idx : longEntrySignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() > 0) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Check if there's a short entry signal
     */
    bool hasShortEntry() const {
        // Check long/short signals first
        for (size_t idx : longShortSignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() < 0) {
                return true;
            }
        }
        // Then check dedicated short entry signals
        for (size_t idx : shortEntrySignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() < 0) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Check if there's a long exit signal
     */
    bool hasLongExit() const {
        for (size_t idx : longExitSignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() != 0) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Check if there's a short exit signal
     */
    bool hasShortExit() const {
        for (size_t idx : shortExitSignals_) {
            if (signals_[idx].signal && signals_[idx].signal->evaluate() != 0) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Get signal count
     */
    size_t size() const { return signals_.size(); }
    
    /**
     * @brief Check if empty
     */
    bool empty() const { return signals_.empty(); }
    
    /**
     * @brief Clear all signals
     */
    void clear() {
        signals_.clear();
        longEntrySignals_.clear();
        shortEntrySignals_.clear();
        longShortSignals_.clear();
        longExitSignals_.clear();
        shortExitSignals_.clear();
    }

private:
    std::vector<SignalConfig> signals_;
    std::vector<size_t> longEntrySignals_;
    std::vector<size_t> shortEntrySignals_;
    std::vector<size_t> longShortSignals_;
    std::vector<size_t> longExitSignals_;
    std::vector<size_t> shortExitSignals_;
};

} // namespace bt
