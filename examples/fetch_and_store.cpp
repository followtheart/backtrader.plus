#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>

#include "bt/data/sqlite_store.hpp"
#include "bt/data/okx_fetcher.hpp"
#include "bt/data/data_ingestor.hpp"
#include "bt/data/types.hpp"

using namespace bt::data;

int main(int argc, char* argv[]) {
#ifndef BT_ENABLE_NETWORKING
    std::cerr << "Error: This example requires BT_ENABLE_NETWORKING to be ON during compilation." << std::endl;
    return 1;
#else

    // 默认配置
    std::string dbPath = "data/metadata.db";
    std::string dataDir = "data/bars";
    std::string symbol = "BTC-USDT";   // OKX Spot
    // std::string symbol = "BTC-USDT-SWAP"; // OKX Perp
    Timeframe tf = {Timeframe::Minute, 1}; // 1m

    if (argc > 1) symbol = argv[1];

    std::cout << 
        "=== OKX Data Fetch & Store Example ===\n"
        "Symbol: " << symbol << "\n"
        "Timeframe: " << tf.toString() << "\n"
        "Database: " << dbPath << "\n"
        "Data Dir: " << dataDir << "\n"
        "======================================\n";

    try {
        // 1. 初始化存储引擎
        auto store = std::make_shared<SQLiteStore>(dbPath, dataDir);
        
        // 注册品种信息 (可选，但在数据库中留痕是好习惯)
        SymbolInfo info;
        info.symbol = symbol;
        info.exchange = "OKX";
        info.type = AssetType::Crypto;
        store->registerSymbol(info);

        // 2. 初始化抓取器
        auto fetcher = std::make_shared<OkxFetcher>();

        // 3. 初始化同步工具
        DataIngestor ingestor(store, fetcher);

        // 如果在中国大陆，设置代理可能需要环境变量
        // _putenv("HTTP_PROXY=http://127.0.0.1:7890");
        // _putenv("HTTPS_PROXY=http://127.0.0.1:7890");

        // 4. 设置进度回调
        ingestor.setProgressCallback([](const std::string& sym, int64_t lastTs) {
            // 将 Unix 毫秒时间戳转换为可读字符串 (这里简单处理)
            time_t raw_time = lastTs / 1000;
            struct tm* time_info = localtime(&raw_time);
            char buffer[80];
            strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", time_info);
            
            std::cout << "[Sync] " << sym << " stored up to: " << buffer << " (" << lastTs << ")" << std::endl;
        });

        std::cout << "Starting synchronization... (Press Ctrl+C to stop if huge)" << std::endl;

        // 5. 执行同步
        // 示例 A: 自动同步到最新 (等同于 downloadRange(symbol, tf, 0, 0))
        // ingestor.sync(symbol, tf);

        // 示例 B: 指定最近 12 小时的范围下载
        // 取当前时间
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        int64_t twelve_hours_ms = 12 * 60 * 60 * 1000;
        int64_t start_ms = now_ms - twelve_hours_ms;

        std::cout << "Downloading range: " << start_ms << " -> " << now_ms << std::endl;
        ingestor.downloadRange(symbol, tf, start_ms, now_ms);

        std::cout << "Synchronization finished." << std::endl;

        // 6. 验证：读取最后 10 根 K 线
        std::cout << "\nVerifying stored data (Last 10 bars):" << std::endl;
        
        int64_t lastStoredTs = store->getLastTimestamp(symbol, tf);
        if (lastStoredTs > 0) {
            // 读取过去 60 分钟的数据 (确保涵盖最后 10 根)
            int64_t startTs = lastStoredTs - (60 * 60 * 1000); 
            auto bars = store->readBars(symbol, tf, startTs, lastStoredTs);

            size_t count = 0;
            size_t total = bars.size();
            size_t startIdx = (total > 10) ? (total - 10) : 0;

            std::cout << std::fixed << std::setprecision(2);
            for (size_t i = startIdx; i < total; ++i) {
                const auto& bar = bars[i];
                time_t rt = bar.timestamp / 1000;
                struct tm* ti = localtime(&rt);
                char tbuf[80];
                strftime(tbuf, 80, "%Y-%m-%d %H:%M:%S", ti);

                std::cout << "[" << tbuf << "] "
                          << "O:" << bar.open << " "
                          << "H:" << bar.high << " "
                          << "L:" << bar.low << " "
                          << "C:" << bar.close << " "
                          << "V:" << bar.volume << std::endl;
            }
            std::cout << "Total bars in last hour range: " << total << std::endl;
        } else {
            std::cout << "No data found." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
#endif
}
