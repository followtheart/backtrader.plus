#pragma once

#include "bt/data/store.hpp"
#include "bt/data/fetcher.hpp"
#include <memory>
#include <functional>

namespace bt {
namespace data {

    class DataIngestor {
    public:
        DataIngestor(std::shared_ptr<DataStore> store, std::shared_ptr<Fetcher> fetcher);
        
        // Start syncing data for the given symbol
        // This will loop and fetch data until caught up
        void sync(const std::string& symbol, const Timeframe& tf);

        // Download data for a specific range [start_ts, end_ts]
        // If end_ts is 0, it downloads until "now" (or api limit)
        // Note: For append-only safety, actual start time will be max(start_ts, last_stored_ts)
        void downloadRange(const std::string& symbol, const Timeframe& tf, int64_t start_ts, int64_t end_ts = 0);

        // Set a hook to be called after each batch write (e.g. for logging progress)
        void setProgressCallback(std::function<void(const std::string&, int64_t)> callback);

    private:
        std::shared_ptr<DataStore> store_;
        std::shared_ptr<Fetcher> fetcher_;
        std::function<void(const std::string&, int64_t)> progress_callback_;
    };

} // namespace data
} // namespace bt
