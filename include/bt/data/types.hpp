#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bt {
namespace data {

    // 资产类型
    enum class AssetType {
        Crypto, // 7x24, e.g. BTC/USDT
        Stock,  // Regular trading hours, e.g. AAPL
        Future, // Expiration dates
        Forex   // 24/5
    };

    // 时间周期
    struct Timeframe {
        enum Unit {
            Minute,
            Hour,
            Day,
            Week,
            Month
        };

        Unit unit;
        int value; // e.g., 5 for 5-minute, 1 for 1-hour

        // Helper for string representation
        std::string toString() const {
            std::string u_str;
            switch(unit) {
                case Minute: u_str = "m"; break;
                case Hour:   u_str = "h"; break;
                case Day:    u_str = "d"; break;
                case Week:   u_str = "w"; break;
                case Month:  u_str = "M"; break;
            }
            return std::to_string(value) + u_str;
        }

        bool operator==(const Timeframe& other) const {
            return unit == other.unit && value == other.value;
        }
    };

    // 存储的 K 线结构 (使用 int64_t 时间戳以保持精度)
    struct StoredBar {
        int64_t timestamp; // Unix timestamp in milliseconds
        double open;
        double high;
        double low;
        double close;
        double volume;
        double open_interest;
    };

    // 品种信息
    struct SymbolInfo {
        std::string symbol;
        std::string exchange;
        AssetType type;
    };

} // namespace data
} // namespace bt
