/**
 * @file datafeed.hpp
 * @brief Data Feed System - Data sources for backtesting
 * 
 * Corresponds to Python's feed.py and feeds/
 */

#pragma once

#include "bt/lineseries.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace bt {

/**
 * @brief DateTime representation
 */
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    
    DateTime() = default;
    
    DateTime(int y, int m, int d, int h = 0, int min = 0, int s = 0)
        : year(y), month(m), day(d), hour(h), minute(min), second(s) {}
    
    // Convert to double (days since epoch)
    double toDouble() const {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        auto time = std::mktime(&tm);
        return static_cast<double>(time) / 86400.0;
    }
    
    // Parse from string "YYYY-MM-DD" or "YYYY-MM-DD HH:MM:SS"
    // dtformat: 0 = auto, 1 = date only (YYYY-MM-DD), 2 = date + time (YYYY-MM-DD HH:MM:SS)
    static DateTime parse(const std::string& str, int dtformat = 0) {
        DateTime dt;
        if (str.length() >= 10) {
            dt.year = std::stoi(str.substr(0, 4));
            dt.month = std::stoi(str.substr(5, 2));
            dt.day = std::stoi(str.substr(8, 2));
        }
        // Parse time component based on dtformat
        // dtformat=0: auto (parse if string is long enough)
        // dtformat=1: date only (skip time parsing)
        // dtformat=2: force parse time
        bool parseTime = (dtformat != 1) && (str.length() >= 19);
        
        if (parseTime) {
            dt.hour = std::stoi(str.substr(11, 2));
            dt.minute = std::stoi(str.substr(14, 2));
            dt.second = std::stoi(str.substr(17, 2));
        }
        return dt;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << year << "-"
            << std::setw(2) << month << "-"
            << std::setw(2) << day;
        if (hour || minute || second) {
            oss << " " << std::setw(2) << hour << ":"
                << std::setw(2) << minute << ":"
                << std::setw(2) << second;
        }
        return oss.str();
    }
};

/**
 * @brief Base DataFeed class
 */
class DataFeed : public OHLCVData {
public:
    // Additional line indices
    static constexpr Size DATETIME = 6;
    
    DataFeed() {
        addLine("datetime");
    }
    
    virtual ~DataFeed() = default;
    
    /**
     * @brief Load data from source
     * @return true if successful
     */
    virtual bool load() = 0;
    
    /**
     * @brief Preload all data
     */
    virtual void preload() { load(); }
    
    /**
     * @brief Reset to beginning
     */
    void reset() {
        currentIdx_ = 0;
        home();
    }
    
    /**
     * @brief Advance to next bar
     * @return true if more data available
     */
    bool next() {
        if (currentIdx_ >= length()) return false;
        advance();
        ++currentIdx_;
        return true;
    }
    
    /**
     * @brief Get datetime line
     */
    LineBuffer& datetime() { return line(DATETIME); }
    const LineBuffer& datetime() const { return line(DATETIME); }
    
    /**
     * @brief Get datetime at index
     */
    DateTime getDateTime(Index idx = 0) const {
        double d = datetime()[idx];
        time_t t = static_cast<time_t>(d * 86400.0);
        std::tm* tm = std::localtime(&t);
        return DateTime(
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec
        );
    }
    
    /**
     * @brief Get data name
     */
    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    /**
     * @brief Get data length
     */
    Size length() const { return close().length(); }
    
    /**
     * @brief Check if data is ready (after minimum period)
     */
    bool ready() const { return OHLCVData::ready(); }

protected:
    std::string name_;
    Size currentIdx_ = 0;
};

/**
 * @brief Generic CSV Data Feed
 * 
 * Flexible CSV parser with configurable column mapping
 */
class GenericCSVData : public DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(datetime, 0)     // Column index for datetime
        BT_PARAM(open, 1)
        BT_PARAM(high, 2)
        BT_PARAM(low, 3)
        BT_PARAM(close, 4)
        BT_PARAM(volume, 5)
        BT_PARAM(openinterest, -1)  // -1 = not present
        BT_PARAM(dtformat, 0)     // 0 = auto, 1 = YYYY-MM-DD, 2 = YYYY-MM-DD HH:MM:SS
        BT_PARAM(header, 1)       // Number of header rows to skip
        BT_PARAM(separator, 0)    // 0 = comma, 1 = tab, 2 = semicolon
    BT_PARAMS_END()
    
    GenericCSVData() = default;
    
    explicit GenericCSVData(const std::string& filepath)
        : filepath_(filepath) {}
    
    void setFilepath(const std::string& filepath) { filepath_ = filepath; }
    const std::string& filepath() const { return filepath_; }
    
    bool load() override {
        std::ifstream file(filepath_);
        if (!file.is_open()) {
            return false;
        }
        
        char sep = ',';
        int sepType = p().get<int>("separator");
        if (sepType == 1) sep = '\t';
        else if (sepType == 2) sep = ';';
        
        int headerRows = p().get<int>("header");
        std::string line;
        
        // Skip header rows
        for (int i = 0; i < headerRows && std::getline(file, line); ++i) {}
        
        // Read data
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::vector<std::string> cols = split(line, sep);
            if (cols.empty()) continue;
            
            try {
                int dtCol = p().get<int>("datetime");
                int oCol = p().get<int>("open");
                int hCol = p().get<int>("high");
                int lCol = p().get<int>("low");
                int cCol = p().get<int>("close");
                int vCol = p().get<int>("volume");
                int oiCol = p().get<int>("openinterest");
                int dtFormat = p().get<int>("dtformat");
                
                DateTime dt = DateTime::parse(cols[dtCol], dtFormat);
                Value o = std::stod(cols[oCol]);
                Value h = std::stod(cols[hCol]);
                Value l = std::stod(cols[lCol]);
                Value c = std::stod(cols[cCol]);
                Value v = (vCol >= 0 && vCol < static_cast<int>(cols.size())) 
                          ? std::stod(cols[vCol]) : 0.0;
                Value oi = (oiCol >= 0 && oiCol < static_cast<int>(cols.size())) 
                           ? std::stod(cols[oiCol]) : 0.0;
                
                addBar(o, h, l, c, v, oi);
                datetime().push(dt.toDouble());
                
            } catch (...) {
                // Skip malformed lines
                continue;
            }
        }
        
        return length() > 0;
    }

private:
    std::string filepath_;
    
    static std::vector<std::string> split(const std::string& str, char sep) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, sep)) {
            // Trim whitespace
            size_t start = item.find_first_not_of(" \t\r\n");
            size_t end = item.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                result.push_back(item.substr(start, end - start + 1));
            } else {
                result.push_back("");
            }
        }
        return result;
    }
};

/**
 * @brief Backtrader CSV format
 * 
 * Format: YYYY-MM-DD, Open, High, Low, Close, Volume, OpenInterest
 */
class BacktraderCSVData : public GenericCSVData {
public:
    BacktraderCSVData() {
        p().set("datetime", 0);
        p().set("open", 1);
        p().set("high", 2);
        p().set("low", 3);
        p().set("close", 4);
        p().set("volume", 5);
        p().set("openinterest", 6);
        p().set("header", 1);
    }
    
    explicit BacktraderCSVData(const std::string& filepath) 
        : BacktraderCSVData() {
        setFilepath(filepath);
    }
};

/**
 * @brief Yahoo Finance CSV format
 */
class YahooFinanceData : public GenericCSVData {
public:
    YahooFinanceData() {
        // Yahoo format: Date,Open,High,Low,Close,Adj Close,Volume
        p().set("datetime", 0);
        p().set("open", 1);
        p().set("high", 2);
        p().set("low", 3);
        p().set("close", 4);
        p().set("volume", 6);
        p().set("openinterest", -1);
        p().set("header", 1);
    }
    
    explicit YahooFinanceData(const std::string& filepath)
        : YahooFinanceData() {
        setFilepath(filepath);
    }
};

/**
 * @brief In-memory data feed (for programmatic data)
 */
class MemoryDataFeed : public DataFeed {
public:
    bool load() override { return true; }  // Already loaded
    
    void addBar(const DateTime& dt, Value o, Value h, Value l, Value c, 
                Value v = 0.0, Value oi = 0.0) {
        OHLCVData::addBar(o, h, l, c, v, oi);
        datetime().push(dt.toDouble());
    }
};

} // namespace bt
