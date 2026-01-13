#pragma once

#include "bt/data/types.hpp"
#include <vector>
#include <string>

namespace bt {
namespace data {

    class DataStore {
    public:
        virtual ~DataStore() = default;

        // 获取上次存储的最后时间戳（毫秒级）
        // 如果没有数据，返回 0
        virtual int64_t getLastTimestamp(const std::string& symbol, const Timeframe& tf) = 0;

        // 写入 K 线数据 (追加模式)
        virtual void writeBars(const std::string& symbol, const Timeframe& tf, const std::vector<StoredBar>& bars) = 0;

        // 读取数据
        // start, end: Unix timestamp in milliseconds
        virtual std::vector<StoredBar> readBars(const std::string& symbol, const Timeframe& tf, int64_t start, int64_t end) = 0;
    };

} // namespace data
} // namespace bt
