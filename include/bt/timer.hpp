/**
 * @file timer.hpp
 * @brief Timer System
 * 
 * Corresponds to Python's timer.py
 * Provides scheduled timer functionality for strategies.
 * 
 * Timers can be used to trigger actions at specific times:
 * - End of day portfolio rebalancing
 * - Scheduled report generation
 * - Periodic position checks
 */

#pragma once

#include "bt/common.hpp"
#include "bt/params.hpp"
#include "bt/datafeed.hpp"
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <set>

namespace bt {

// Forward declarations
class Strategy;

/**
 * @brief Time of day representation
 */
struct TimeOfDay {
    int hour = 0;
    int minute = 0;
    int second = 0;
    
    TimeOfDay() = default;
    TimeOfDay(int h, int m, int s = 0) : hour(h), minute(m), second(s) {}
    
    // Convert to minutes from midnight
    int toMinutes() const { return hour * 60 + minute; }
    
    // Convert to seconds from midnight
    int toSeconds() const { return hour * 3600 + minute * 60 + second; }
    
    // Parse from string "HH:MM" or "HH:MM:SS"
    static TimeOfDay parse(const std::string& str) {
        TimeOfDay tod;
        if (str.length() >= 5) {
            tod.hour = std::stoi(str.substr(0, 2));
            tod.minute = std::stoi(str.substr(3, 2));
        }
        if (str.length() >= 8) {
            tod.second = std::stoi(str.substr(6, 2));
        }
        return tod;
    }
    
    bool operator==(const TimeOfDay& other) const {
        return hour == other.hour && minute == other.minute && second == other.second;
    }
    
    bool operator<(const TimeOfDay& other) const {
        return toSeconds() < other.toSeconds();
    }
    
    bool operator<=(const TimeOfDay& other) const {
        return toSeconds() <= other.toSeconds();
    }
    
    bool operator>(const TimeOfDay& other) const {
        return toSeconds() > other.toSeconds();
    }
    
    bool operator>=(const TimeOfDay& other) const {
        return toSeconds() >= other.toSeconds();
    }
};

/**
 * @brief Timer class
 * 
 * A timer triggers at specified times and notifies the strategy.
 * Supports:
 * - Single shot or repeating timers
 * - Day of week filtering
 * - Day of month filtering
 * - Time offset
 */
class Timer {
public:
    /**
     * @brief Timer parameters
     */
    struct Params {
        int tid = -1;                       ///< Timer ID (auto-assigned if -1)
        TimeOfDay when;                     ///< Time of day to trigger
        int offsetMinutes = 0;              ///< Offset in minutes (can be negative)
        int repeatMinutes = 0;              ///< Repeat interval (0 = no repeat)
        std::set<int> weekdays;             ///< Days of week (1=Mon, 7=Sun), empty = all
        bool weekcarry = false;             ///< Carry to next week if missed
        std::set<int> monthdays;            ///< Days of month (1-31), empty = all
        bool monthcarry = true;             ///< Carry to next month if missed
        bool cheat = false;                 ///< Trigger before bar processing
    };
    
    Timer() = default;
    explicit Timer(int tid) { params_.tid = tid; }
    Timer(int tid, const TimeOfDay& when) { 
        params_.tid = tid; 
        params_.when = when;
    }
    
    // Getters
    int id() const { return params_.tid; }
    const TimeOfDay& when() const { return params_.when; }
    bool isCheat() const { return params_.cheat; }
    
    // Setters (builder pattern)
    Timer& setId(int id) { params_.tid = id; return *this; }
    Timer& setWhen(const TimeOfDay& w) { params_.when = w; return *this; }
    Timer& setWhen(int h, int m, int s = 0) { 
        params_.when = TimeOfDay(h, m, s); 
        return *this; 
    }
    Timer& setOffset(int minutes) { params_.offsetMinutes = minutes; return *this; }
    Timer& setRepeat(int minutes) { params_.repeatMinutes = minutes; return *this; }
    Timer& setWeekdays(const std::set<int>& days) { params_.weekdays = days; return *this; }
    Timer& addWeekday(int day) { params_.weekdays.insert(day); return *this; }
    Timer& setWeekcarry(bool carry) { params_.weekcarry = carry; return *this; }
    Timer& setMonthdays(const std::set<int>& days) { params_.monthdays = days; return *this; }
    Timer& addMonthday(int day) { params_.monthdays.insert(day); return *this; }
    Timer& setMonthcarry(bool carry) { params_.monthcarry = carry; return *this; }
    Timer& setCheat(bool c) { params_.cheat = c; return *this; }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }
    
    /**
     * @brief Check if timer should trigger at given datetime
     * 
     * @param dt Current datetime
     * @return true if timer should trigger
     */
    bool check(const DateTime& dt) {
        // Check weekday filter
        if (!params_.weekdays.empty()) {
            // Calculate day of week (1=Mon, 7=Sun)
            // Simplified: would need proper date calculation
            int dow = getDayOfWeek(dt);
            if (params_.weekdays.find(dow) == params_.weekdays.end()) {
                // Not a valid weekday
                if (!params_.weekcarry) return false;
                // With weekcarry, would need to check if we should trigger anyway
            }
        }
        
        // Check monthday filter
        if (!params_.monthdays.empty()) {
            if (params_.monthdays.find(dt.day) == params_.monthdays.end()) {
                if (!params_.monthcarry) return false;
            }
        }
        
        // Calculate trigger time with offset
        int triggerMinutes = params_.when.toMinutes() + params_.offsetMinutes;
        int currentMinutes = dt.hour * 60 + dt.minute;
        
        // Check if we've passed the trigger time
        if (currentMinutes >= triggerMinutes && !triggered_) {
            triggered_ = true;
            lastTrigger_ = dt;
            
            // Reset for repeat
            if (params_.repeatMinutes > 0) {
                nextTriggerMinutes_ = triggerMinutes + params_.repeatMinutes;
            }
            
            return true;
        }
        
        // Check for repeat
        if (triggered_ && params_.repeatMinutes > 0) {
            if (currentMinutes >= nextTriggerMinutes_) {
                nextTriggerMinutes_ += params_.repeatMinutes;
                lastTrigger_ = dt;
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * @brief Reset timer for new day
     */
    void resetDaily() {
        triggered_ = false;
        nextTriggerMinutes_ = 0;
    }
    
    /**
     * @brief Get last trigger time
     */
    const DateTime& lastTrigger() const { return lastTrigger_; }

private:
    /**
     * @brief Calculate day of week (1=Monday, 7=Sunday)
     * 
     * Simplified implementation using Zeller's congruence
     */
    static int getDayOfWeek(const DateTime& dt) {
        int y = dt.year;
        int m = dt.month;
        int d = dt.day;
        
        // Adjust for January/February
        if (m < 3) {
            m += 12;
            y--;
        }
        
        // Zeller's congruence
        int k = y % 100;
        int j = y / 100;
        int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
        
        // Convert to ISO week (1=Mon, 7=Sun)
        // h: 0=Sat, 1=Sun, 2=Mon, 3=Tue, 4=Wed, 5=Thu, 6=Fri
        int iso = ((h + 5) % 7) + 1;
        return iso;
    }
    
    Params params_;
    bool triggered_ = false;
    int nextTriggerMinutes_ = 0;
    DateTime lastTrigger_;
};

/**
 * @brief Timer manager
 * 
 * Manages multiple timers and handles their execution.
 */
class TimerManager {
public:
    /**
     * @brief Add a timer
     * 
     * @param timer Timer to add
     * @return Timer ID
     */
    int addTimer(Timer timer) {
        if (timer.id() < 0) {
            timer.setId(nextId_++);
        }
        timers_.push_back(std::move(timer));
        return timers_.back().id();
    }
    
    /**
     * @brief Add a timer with convenience parameters
     * 
     * @param when Time of day to trigger
     * @param offsetMinutes Offset from trigger time
     * @param repeatMinutes Repeat interval
     * @return Timer ID
     */
    int addTimer(const TimeOfDay& when, int offsetMinutes = 0, int repeatMinutes = 0) {
        Timer t(nextId_++);
        t.setWhen(when)
         .setOffset(offsetMinutes)
         .setRepeat(repeatMinutes);
        timers_.push_back(std::move(t));
        return timers_.back().id();
    }
    
    /**
     * @brief Remove a timer
     * 
     * @param tid Timer ID to remove
     * @return true if removed
     */
    bool removeTimer(int tid) {
        auto it = std::find_if(timers_.begin(), timers_.end(),
                               [tid](const Timer& t) { return t.id() == tid; });
        if (it != timers_.end()) {
            timers_.erase(it);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Get a timer by ID
     * 
     * @param tid Timer ID
     * @return Pointer to timer, or nullptr if not found
     */
    Timer* getTimer(int tid) {
        auto it = std::find_if(timers_.begin(), timers_.end(),
                               [tid](const Timer& t) { return t.id() == tid; });
        return (it != timers_.end()) ? &(*it) : nullptr;
    }
    
    /**
     * @brief Check all timers against current datetime
     * 
     * Returns list of timer IDs that should trigger.
     * 
     * @param dt Current datetime
     * @param cheatPhase true if checking for cheat timers
     * @return Vector of triggered timer IDs
     */
    std::vector<int> check(const DateTime& dt, bool cheatPhase = false) {
        std::vector<int> triggered;
        
        // Check if we've moved to a new day
        if (dt.day != lastDay_) {
            for (auto& t : timers_) {
                t.resetDaily();
            }
            lastDay_ = dt.day;
        }
        
        // Check each timer
        for (auto& t : timers_) {
            // Filter by cheat phase
            if (t.isCheat() != cheatPhase) continue;
            
            if (t.check(dt)) {
                triggered.push_back(t.id());
            }
        }
        
        return triggered;
    }
    
    /**
     * @brief Get all timers
     */
    const std::vector<Timer>& timers() const { return timers_; }
    
    /**
     * @brief Get number of timers
     */
    size_t size() const { return timers_.size(); }
    
    /**
     * @brief Clear all timers
     */
    void clear() {
        timers_.clear();
        nextId_ = 0;
    }

private:
    std::vector<Timer> timers_;
    int nextId_ = 0;
    int lastDay_ = -1;  // Track day changes for reset
};

/**
 * @brief Pre-defined timer schedules
 */
namespace schedule {

/**
 * @brief Create timer for market open (9:30 AM)
 */
inline Timer marketOpen(int offsetMinutes = 0) {
    return Timer().setWhen(9, 30).setOffset(offsetMinutes);
}

/**
 * @brief Create timer for market close (4:00 PM)
 */
inline Timer marketClose(int offsetMinutes = 0) {
    return Timer().setWhen(16, 0).setOffset(offsetMinutes);
}

/**
 * @brief Create timer for end of day (after market close)
 */
inline Timer endOfDay(int offsetMinutes = 15) {
    return Timer().setWhen(16, 0).setOffset(offsetMinutes);
}

/**
 * @brief Create hourly timer
 */
inline Timer hourly(int startHour = 9, int endHour = 16) {
    return Timer().setWhen(startHour, 0).setRepeat(60);
}

/**
 * @brief Create timer for first trading day of month
 */
inline Timer monthStart(const TimeOfDay& when = TimeOfDay(9, 30)) {
    Timer t;
    t.setWhen(when);
    t.addMonthday(1);
    t.addMonthday(2);
    t.addMonthday(3);  // In case 1st is weekend
    t.setMonthcarry(true);
    return t;
}

/**
 * @brief Create timer for last trading day of month
 */
inline Timer monthEnd(const TimeOfDay& when = TimeOfDay(15, 30)) {
    Timer t;
    t.setWhen(when);
    // Would need to calculate last trading day
    // Simplified: use end-of-month dates
    for (int d = 28; d <= 31; ++d) {
        t.addMonthday(d);
    }
    return t;
}

/**
 * @brief Create timer for specific weekdays
 * 
 * @param dow Day of week (1=Mon, 7=Sun)
 * @param when Time of day
 */
inline Timer weekday(int dow, const TimeOfDay& when = TimeOfDay(9, 30)) {
    return Timer().setWhen(when).addWeekday(dow);
}

/**
 * @brief Create timer for Monday only
 */
inline Timer monday(const TimeOfDay& when = TimeOfDay(9, 30)) {
    return weekday(1, when);
}

/**
 * @brief Create timer for Friday only
 */
inline Timer friday(const TimeOfDay& when = TimeOfDay(15, 30)) {
    return weekday(5, when);
}

} // namespace schedule

} // namespace bt
