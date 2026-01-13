#pragma once

#include "bt/data/store.hpp"
#include <string>
#include <filesystem>
#include <mutex>

namespace bt {
namespace data {

    class FlatFileStore : public DataStore {
    public:
        explicit FlatFileStore(const std::string& baseDir);
        ~FlatFileStore() override = default;

        int64_t getLastTimestamp(const std::string& symbol, const Timeframe& tf) override;
        void writeBars(const std::string& symbol, const Timeframe& tf, const std::vector<StoredBar>& bars) override;
        std::vector<StoredBar> readBars(const std::string& symbol, const Timeframe& tf, int64_t start, int64_t end) override;

    private:
        std::string base_dir_;
        std::mutex file_mutex_; // Basic thread safety

        std::filesystem::path getFilePath(const std::string& symbol, const Timeframe& tf);
    };

} // namespace data
} // namespace bt
