#pragma once

#include "bt/data/types.hpp"
#include <vector>
#include <string>

namespace bt {
namespace data {

    class Fetcher {
    public:
        virtual ~Fetcher() = default;

        // Fetch bars starting from 'afterTimestamp' (exclusive) to 'untilTimestamp' (inclusive or default now)
        // limit: max number of bars to return per call (pagination)
        virtual std::vector<StoredBar> fetch(
            const std::string& symbol, 
            const Timeframe& tf, 
            int64_t afterTimestamp, 
            int limit = 100
        ) = 0;
        
        // Return exchange name
        virtual std::string getName() const = 0;
    };

} // namespace data
} // namespace bt
