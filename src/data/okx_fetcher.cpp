#include "bt/data/okx_fetcher.hpp"
#include <algorithm>
#include <iostream>

#ifdef BT_ENABLE_NETWORKING
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#endif

namespace bt {
namespace data {

    OkxFetcher::OkxFetcher() {}

    std::string OkxFetcher::convertTimeframe(const Timeframe& tf) {
        // OKX format: 1m, 3m, 5m, 15m, 30m, 1H, 2H, 4H, 6D, 1D, 1W, 1M
        if (tf.unit == Timeframe::Minute) {
            return std::to_string(tf.value) + "m";
        } else if (tf.unit == Timeframe::Hour) {
            return std::to_string(tf.value) + "H";
        } else if (tf.unit == Timeframe::Day) {
            return std::to_string(tf.value) + "D";
        } else if (tf.unit == Timeframe::Week) {
            return std::to_string(tf.value) + "W";
        } else if (tf.unit == Timeframe::Month) {
            return std::to_string(tf.value) + "M";
        }
        return "1m";
    }

    std::vector<StoredBar> OkxFetcher::fetch(
            const std::string& symbol, 
            const Timeframe& tf, 
            int64_t afterTimestamp, 
            int limit
    ) {
        std::vector<StoredBar> bars;

#ifdef BT_ENABLE_NETWORKING
        // OKX API: /api/v5/market/history-candles
        // Response: [ts, o, h, l, c, vol, volCcy, volCcyQuote, confirm]
        // "after" param in OKX API is for pagination requesting older data (id-based or time-based desc).
        // Wait, OKX usually returns data in DESCENDING order (newest first).
        // If we want [afterTimestamp, future], we should use 'before' param or no param?
        // Actually, to sync FORWARD (historical -> now):
        // We usually request data 'after' the last known timestamp.
        // OKX 'after' param: "Request data older than this timestamp". This is for walking backwards.
        // OKX 'before' param: "Request data newer than this timestamp". This is for walking forwards.
        
        // So, if we have last_ts (e.g. yesterday), we want data NEWER than yesterday. 
        // We should use `afterTimestamp` as value for `after` parameter? No.
        // OKX API definition: 
        // `after`: Retrieve records with timestamp OLDER than this value. (Pagination for going back)
        // `before`: Retrieve records with timestamp NEWER than this value. (Pagination for going forward)
        // Correct usage for forward sync:
        // Use `before` = lastTimestamp. 
        // NOTE: OKX returns bars sorted New -> Old. 
        // So fetching with `before=last_ts` returns bars that are *newer* than last_ts, but still sorted New->Old in the list.
        // We will receive [Newest .... Oldest( > last_ts )]
        
        std::string bar = convertTimeframe(tf);
        // Use proxy-friendly URL or ensure system proxy is set if accessing from restricted region
        // Alternately, use AWS API if available: https://aws.okx.com
        std::string url = "https://www.okx.com/api/v5/market/history-candles";
        
        // If lastTimestamp is 0, we might want to start from a long time ago. 
        // OKX history-candles might require a start point or just gives limits.
        // If we want distinct forward fill, it's tricky with OKX's history-candles which is designed for backfill.
        // Let's assume we use regular `candles` if close to now, or `history-candles` paging.
        
        // Strategy: 
        // We want to fetch bars chronologically ascending.
        // But OKX gives descending.
        // If we provide `after` (older than), we go back in time.
        // If we provide `before` (newer than), we go forward in time.
        
        cpr::Parameters params{
            {"instId", symbol},
            {"bar", bar},
            {"limit", std::to_string(limit)}
        };

        if (afterTimestamp > 0) {
            // OKX API strategy for "Forward Sync":
            // We want [afterTimestamp, ...].
            // `history-candles` default sort is DESC (New -> Old).
            // `after` = Older than X.
            // `before` = Newer than X. (But returns LATEST chunk > X).

            // To get the chunk immediately following `afterTimestamp`:
            // We calculate a target timestamp `target = afterTimestamp + (Limit * Interval)`.
            // Then we use `after = target`.
            // OKX returns bars OLDER than target.
            // Since it sorts DESC, it returns bars closest to target, going backwards.
            // i.e. [target-1, target-2, ... afterTimestamp].
            // This effectively gives us the window we want.

            int64_t intervalMs = 0;
            if (tf.unit == Timeframe::Minute) intervalMs = tf.value * 60 * 1000;
            else if (tf.unit == Timeframe::Hour) intervalMs = tf.value * 3600 * 1000;
            else if (tf.unit == Timeframe::Day) intervalMs = (int64_t)tf.value * 24 * 3600 * 1000;
            else if (tf.unit == Timeframe::Week) intervalMs = (int64_t)tf.value * 7 * 24 * 3600 * 1000;
            else if (tf.unit == Timeframe::Month) intervalMs = (int64_t)tf.value * 30 * 24 * 3600 * 1000; 
            else intervalMs = 60000;

            // We ask for a window slightly larger than limit to account for potential gaps,
            // but not too large to skip data. Assuming Crypto is 24/7 continuous mostly.
            int safeLimit = (limit > 100) ? 100 : limit; 
            
            // Target Future Timestamp = Start + (100 * interval).
            // Request `after` = Target. 
            // Returns Top 100 bars < Target.
            // Ideally: [Target-interval ... Start+interval]
            int64_t targetFuture = afterTimestamp + ((int64_t)safeLimit * intervalMs); 
            
            params.Add({"after", std::to_string(targetFuture)});
        }

        cpr::Response r = cpr::Get(cpr::Url{url}, params);

        if (r.status_code == 200) {
            try {
                auto json = nlohmann::json::parse(r.text);
                if (json["code"] == "0") {
                    auto data = json["data"];
                    for (const auto& item : data) {
                        StoredBar b;
                        // OKX: [ts, o, h, l, c, vol, ...]
                        // ts is string milliseconds
                        b.timestamp = std::stoll(item[0].get<std::string>());
                        b.open = std::stod(item[1].get<std::string>());
                        b.high = std::stod(item[2].get<std::string>());
                        b.low = std::stod(item[3].get<std::string>());
                        b.close = std::stod(item[4].get<std::string>());
                        b.volume = std::stod(item[5].get<std::string>()); // vol in base ccy
                        b.open_interest = 0; // Not provided in basic candles

                        bars.push_back(b);
                    }

                    // OKX returns descending (New -> Old).
                    // We need Ascending (Old -> New) for appending to file.
                    std::reverse(bars.begin(), bars.end());
                }
            } catch (const std::exception& e) {
                std::cerr << "JSON Parse Error: " << e.what() << std::endl;
            }
        } else {
             std::cerr << "HTTP Error: " << r.status_code << " (" << r.error.message << ")" << std::endl;
        }
#else
        std::cerr << "Networking disabled. Please compile with BT_ENABLE_NETWORKING=ON." << std::endl;
#endif

        return bars;
    }

} // namespace data
} // namespace bt
