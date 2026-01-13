#pragma once

#include "bt/data/fetcher.hpp"
#include <string>

namespace bt {
namespace data {

    class OkxFetcher : public Fetcher {
    public:
        OkxFetcher();
        ~OkxFetcher() override = default;

        std::vector<StoredBar> fetch(
            const std::string& symbol, 
            const Timeframe& tf, 
            int64_t afterTimestamp, 
            int limit = 100
        ) override;

        std::string getName() const override { return "OKX"; }
    
    private:
        std::string convertTimeframe(const Timeframe& tf);
    };

} // namespace data
} // namespace bt
