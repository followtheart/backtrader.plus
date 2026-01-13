#pragma once

#include "bt/feed/datafeed.hpp"
#include "bt/data/store.hpp"
#include <string>
#include <memory>

namespace bt {
namespace data {

    class StorageFeed : public bt::DataFeed {
    public:
        // Constructor that accepts the DataStore and sync requirements
        // from/to dates are optional (0 = start/now)
        StorageFeed(std::shared_ptr<DataStore> store, 
                   const std::string& symbol, 
                   const Timeframe& tf,
                   int64_t from_ms = 0,
                   int64_t to_ms = 0);

        ~StorageFeed() override = default;

        // Override load to fetch data from store and populate lines
        bool load() override;

    private:
        std::shared_ptr<DataStore> store_;
        std::string symbol_;
        Timeframe tf_;
        int64_t from_ms_;
        int64_t to_ms_;
        
        // Helper to convert unix ms (int64) to bt::DateTime (double days since epoch)
        double convertTimestamp(int64_t ms) const;
    };

} // namespace data
} // namespace bt
