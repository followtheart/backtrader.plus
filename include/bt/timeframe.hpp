/**
 * @file timeframe.hpp
 * @brief TimeFrame definitions and utilities
 * 
 * Corresponds to Python's TimeFrame in resamplerfilter.py
 */

#pragma once

#include "bt/common.hpp"
#include <string>
#include <chrono>
#include <ctime>

namespace bt {

/**
 * @brief TimeFrame enumeration
 * 
 * Defines the various timeframes for data bars.
 * Lower numeric values represent smaller timeframes.
 * 
 * The hierarchy is:
 * NoTimeFrame < Ticks < MicroSeconds < Seconds < Minutes < Days < Weeks < Months < Years
 */
enum class TimeFrame {
    NoTimeFrame = 0,
    Ticks = 1,
    MicroSeconds = 2,
    Seconds = 3,
    Minutes = 4,
    Days = 5,
    Weeks = 6,
    Months = 7,
    Years = 8
};

/**
 * @brief TimeFrame utility functions
 */
namespace timeframe {

/**
 * @brief Get the name of a timeframe
 */
inline std::string name(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::NoTimeFrame: return "NoTimeFrame";
        case TimeFrame::Ticks: return "Ticks";
        case TimeFrame::MicroSeconds: return "MicroSeconds";
        case TimeFrame::Seconds: return "Seconds";
        case TimeFrame::Minutes: return "Minutes";
        case TimeFrame::Days: return "Days";
        case TimeFrame::Weeks: return "Weeks";
        case TimeFrame::Months: return "Months";
        case TimeFrame::Years: return "Years";
        default: return "Unknown";
    }
}

/**
 * @brief Get short name for timeframe (for display)
 */
inline std::string shortName(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::NoTimeFrame: return "";
        case TimeFrame::Ticks: return "T";
        case TimeFrame::MicroSeconds: return "us";
        case TimeFrame::Seconds: return "S";
        case TimeFrame::Minutes: return "M";
        case TimeFrame::Days: return "D";
        case TimeFrame::Weeks: return "W";
        case TimeFrame::Months: return "Mo";
        case TimeFrame::Years: return "Y";
        default: return "?";
    }
}

/**
 * @brief Parse timeframe from string
 */
inline TimeFrame parse(const std::string& s) {
    if (s == "Ticks" || s == "T" || s == "tick") return TimeFrame::Ticks;
    if (s == "MicroSeconds" || s == "us") return TimeFrame::MicroSeconds;
    if (s == "Seconds" || s == "S" || s == "second") return TimeFrame::Seconds;
    if (s == "Minutes" || s == "M" || s == "minute" || s == "min") return TimeFrame::Minutes;
    if (s == "Days" || s == "D" || s == "day" || s == "daily") return TimeFrame::Days;
    if (s == "Weeks" || s == "W" || s == "week" || s == "weekly") return TimeFrame::Weeks;
    if (s == "Months" || s == "Mo" || s == "month" || s == "monthly") return TimeFrame::Months;
    if (s == "Years" || s == "Y" || s == "year" || s == "yearly") return TimeFrame::Years;
    return TimeFrame::NoTimeFrame;
}

/**
 * @brief Check if a timeframe is intraday (smaller than Days)
 */
inline bool isIntraday(TimeFrame tf) {
    return tf < TimeFrame::Days;
}

/**
 * @brief Check if a timeframe is sub-minute
 */
inline bool isSubMinute(TimeFrame tf) {
    return tf < TimeFrame::Minutes;
}

/**
 * @brief Get the duration in seconds for a timeframe with compression
 * 
 * @param tf The timeframe
 * @param compression The compression factor (e.g., 5 for 5-minute bars)
 * @return Duration in seconds (0 for Ticks and variable length periods)
 */
inline int64_t durationSeconds(TimeFrame tf, int compression = 1) {
    switch (tf) {
        case TimeFrame::MicroSeconds: return 0;  // Sub-second
        case TimeFrame::Seconds: return compression;
        case TimeFrame::Minutes: return compression * 60;
        case TimeFrame::Days: return compression * 86400;
        case TimeFrame::Weeks: return compression * 7 * 86400;
        case TimeFrame::Months: return compression * 30 * 86400;  // Approximate
        case TimeFrame::Years: return compression * 365 * 86400;  // Approximate
        default: return 0;
    }
}

/**
 * @brief Compare two timeframes
 * @return true if tf1 is smaller (more granular) than tf2
 */
inline bool isSmaller(TimeFrame tf1, TimeFrame tf2) {
    return static_cast<int>(tf1) < static_cast<int>(tf2);
}

/**
 * @brief Compare two timeframes
 * @return true if tf1 is larger than tf2
 */
inline bool isLarger(TimeFrame tf1, TimeFrame tf2) {
    return static_cast<int>(tf1) > static_cast<int>(tf2);
}

} // namespace timeframe

/**
 * @brief TimeFrame configuration for data resampling
 */
struct TimeFrameConfig {
    TimeFrame timeframe = TimeFrame::Days;
    int compression = 1;  // Bar compression (e.g., 5 for 5-minute bars)
    
    TimeFrameConfig() = default;
    TimeFrameConfig(TimeFrame tf, int comp = 1) 
        : timeframe(tf), compression(comp) {}
    
    /**
     * @brief Get human readable description
     */
    std::string toString() const {
        if (compression == 1) {
            return timeframe::name(timeframe);
        }
        return std::to_string(compression) + " " + timeframe::name(timeframe);
    }
    
    /**
     * @brief Get short description (e.g., "5M" for 5 minutes)
     */
    std::string shortString() const {
        return std::to_string(compression) + timeframe::shortName(timeframe);
    }
    
    /**
     * @brief Compare timeframe configs
     * Smaller configs have smaller timeframes, or same timeframe with smaller compression
     */
    bool operator<(const TimeFrameConfig& other) const {
        if (timeframe != other.timeframe) {
            return timeframe::isSmaller(timeframe, other.timeframe);
        }
        return compression < other.compression;
    }
    
    bool operator==(const TimeFrameConfig& other) const {
        return timeframe == other.timeframe && compression == other.compression;
    }
    
    bool operator!=(const TimeFrameConfig& other) const {
        return !(*this == other);
    }
};

/**
 * @brief DateTime utilities for bar boundary detection
 */
namespace datetime_utils {

/**
 * @brief Get the start of the day
 */
inline std::tm startOfDay(const std::tm& t) {
    std::tm result = t;
    result.tm_hour = 0;
    result.tm_min = 0;
    result.tm_sec = 0;
    return result;
}

/**
 * @brief Get the start of the week (Monday)
 */
inline std::tm startOfWeek(const std::tm& t) {
    std::tm result = startOfDay(t);
    // tm_wday is days since Sunday (0-6)
    int daysFromMonday = (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
    // Subtract days to get to Monday
    result.tm_mday -= daysFromMonday;
    std::mktime(&result);  // Normalize
    return result;
}

/**
 * @brief Get the start of the month
 */
inline std::tm startOfMonth(const std::tm& t) {
    std::tm result = startOfDay(t);
    result.tm_mday = 1;
    std::mktime(&result);  // Normalize
    return result;
}

/**
 * @brief Get the start of the year
 */
inline std::tm startOfYear(const std::tm& t) {
    std::tm result = startOfDay(t);
    result.tm_mon = 0;   // January
    result.tm_mday = 1;
    std::mktime(&result);  // Normalize
    return result;
}

/**
 * @brief Check if given time is at a bar boundary for the timeframe
 * 
 * @param t The time to check
 * @param tf The timeframe
 * @param compression The compression factor
 * @return true if at a boundary
 */
inline bool isAtBoundary(const std::tm& t, TimeFrame tf, int compression = 1) {
    switch (tf) {
        case TimeFrame::Seconds:
            return (t.tm_sec % compression) == 0;
        case TimeFrame::Minutes:
            return (t.tm_sec == 0) && (t.tm_min % compression) == 0;
        case TimeFrame::Days:
            return (t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0);
        case TimeFrame::Weeks:
            // Monday at midnight
            return (t.tm_wday == 1 && t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0);
        case TimeFrame::Months:
            // First day of month at midnight
            return (t.tm_mday == 1 && t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0);
        case TimeFrame::Years:
            // Jan 1st at midnight
            return (t.tm_mon == 0 && t.tm_mday == 1 && 
                    t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0);
        default:
            return true;
    }
}

/**
 * @brief Get minute of day (0-1439)
 */
inline int minuteOfDay(const std::tm& t) {
    return t.tm_hour * 60 + t.tm_min;
}

/**
 * @brief Get second of day (0-86399)
 */
inline int secondOfDay(const std::tm& t) {
    return t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
}

/**
 * @brief Get day of week (0 = Monday, 6 = Sunday)
 * Note: Different from tm_wday which has 0 = Sunday
 */
inline int dayOfWeekISO(const std::tm& t) {
    return (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
}

} // namespace datetime_utils

} // namespace bt
