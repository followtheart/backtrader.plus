#include "bt/data/storage_feed.hpp"
#include <limits>
#include <iostream>

namespace bt {
namespace data {

    StorageFeed::StorageFeed(std::shared_ptr<DataStore> store, 
                             const std::string& symbol, 
                             const Timeframe& tf,
                             int64_t from_ms,
                             int64_t to_ms)
        : store_(std::move(store)), 
          symbol_(symbol), 
          tf_(tf), 
          from_ms_(from_ms), 
          to_ms_(to_ms) {
    }

    double StorageFeed::convertTimestamp(int64_t ms) const {
        // Backtrader C++ uses days since Unix Epoch (1970-01-01)
        // 1 day = 86400 seconds = 86,400,000 milliseconds
        return (double)ms / 86400000.0;
    }

    bool StorageFeed::load() {
        if (!store_) return false;

        int64_t start = (from_ms_ > 0) ? from_ms_ : 0;
        int64_t end = (to_ms_ > 0) ? to_ms_ : std::numeric_limits<int64_t>::max();

        auto bars = store_->readBars(symbol_, tf_, start, end);
        
        if (bars.empty()) {
            return false;
        }

        // Reserve memory to avoid repeated reallocations
        size_t size = bars.size();
        // Assuming DataFeed methods allow hinting capacity, 
        // but typically Lines are dynamic. LineBuffer uses std::vector so we just push.

        for (const auto& bar : bars) {
            // 1. Convert Timestamp
            double dt_val = convertTimestamp(bar.timestamp);
            
            // 2. Push standard OHLCV
            // DataFeed helper method: addBar(open, high, low, close, volume, openInterest)
            addBar(bar.open, bar.high, bar.low, bar.close, bar.volume, bar.open_interest);
            
            // 3. Push DateTime explicitly
            datetime().push(dt_val);
        }

        return length() > 0;
    }

} // namespace data
} // namespace bt
