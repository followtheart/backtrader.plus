#include "bt/data/data_ingestor.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace bt {
namespace data {

    DataIngestor::DataIngestor(std::shared_ptr<DataStore> store, std::shared_ptr<Fetcher> fetcher)
        : store_(std::move(store)), fetcher_(std::move(fetcher)) {}

    void DataIngestor::setProgressCallback(std::function<void(const std::string&, int64_t)> callback) {
        progress_callback_ = std::move(callback);
    }

    void DataIngestor::sync(const std::string& symbol, const Timeframe& tf) {
        // sync is just downloading from the last known point to infinity (0 means no limit)
        // We pass 0 as start_ts, but downloadRange will internally upgrade it to lastStoredTs
        downloadRange(symbol, tf, 0, 0);
    }

    void DataIngestor::downloadRange(const std::string& symbol, const Timeframe& tf, int64_t start_ts, int64_t end_ts) {
        if (!store_ || !fetcher_) return;

        // 1. Get last timestamp from store to ensure continuity
        int64_t lastStored = store_->getLastTimestamp(symbol, tf);
        
        // Effective start is the later of requested start vs what we already have
        // This enforces Append-Only integrity.
        // Exception: If lastStored is 0 (empty DB), we use user's start_ts.
        int64_t currentCursor;
        if (lastStored > 0) {
            currentCursor = std::max(start_ts, lastStored);
             if (start_ts > 0 && start_ts < lastStored) {
                std::cout << "[Warn] Requested start " << start_ts 
                          << " is before stored data end " << lastStored 
                          << ". Skipping overlapping range." << std::endl;
            }
        } else {
            currentCursor = start_ts;
        }

        // If end_ts is provided and we are already past it, nothing to do
        if (end_ts > 0 && currentCursor >= end_ts) {
            std::cout << "Data already covers up to " << end_ts << std::endl;
            return;
        }

        const int batchSize = 100;
        int emptyBatches = 0;

        while (true) {
            // Check range limit
            if (end_ts > 0 && currentCursor >= end_ts) {
                break;
            }

            // Fetch next batch
            auto bars = fetcher_->fetch(symbol, tf, currentCursor, batchSize);

            if (bars.empty()) {
                emptyBatches++;
                if (emptyBatches >= 2) break; // Retry once or stop
            } else {
                emptyBatches = 0;
                
                // Filter bars that exceed end_ts (if set)
                std::vector<StoredBar> barsToWrite;
                barsToWrite.reserve(bars.size());
                
                for(const auto& b : bars) {
                     // Strict > currentCursor to avoid duplicate key if fetcher includes boundary
                     // <= end_ts if limit is set
                     if (b.timestamp > currentCursor && (end_ts == 0 || b.timestamp <= end_ts)) {
                         barsToWrite.push_back(b);
                     }
                }

                if (barsToWrite.empty()) {
                    // All fetched bars were duplicates or out of range
                    // Check if we just reached the end of available data?
                    // If fetcher returned bars but we filtered them all, it means fetcher returned old data 
                    // or data beyond end_ts.
                    
                    // If bars.back().timestamp > end_ts, we are done.
                    if (end_ts > 0 && bars.back().timestamp > end_ts) {
                        break;
                    }
                    
                    // Otherwise, maybe just moved cursor slightly?
                    if (!bars.empty()) {
                        currentCursor = std::max(currentCursor, bars.back().timestamp);
                    }
                } else {
                    // Write
                    store_->writeBars(symbol, tf, barsToWrite);
                    
                    // Update cursor to the absolute last written
                    currentCursor = barsToWrite.back().timestamp;

                    if (progress_callback_) {
                        progress_callback_(symbol, currentCursor);
                    }
                }

                // If fewer than limit, likely no more data from API
                if (bars.size() < static_cast<size_t>(batchSize)) {
                    break;
                }
            }
            
            // Rate limit protection
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

} // namespace data
} // namespace bt
