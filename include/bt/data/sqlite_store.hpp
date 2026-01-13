#pragma once

#include "bt/data/store.hpp"
#include "bt/data/flat_file_store.hpp"
#include <string>
#include <memory>
#include <mutex>

// Forward declaration for sqlite3
struct sqlite3;

namespace bt {
namespace data {

    class SQLiteStore : public DataStore {
    public:
        // dbPath: path to sqlite database file (e.g., "data/metadata.db")
        // dataDir: path to directory for binary files
        SQLiteStore(const std::string& dbPath, const std::string& dataDir);
        ~SQLiteStore() override;

        int64_t getLastTimestamp(const std::string& symbol, const Timeframe& tf) override;
        void writeBars(const std::string& symbol, const Timeframe& tf, const std::vector<StoredBar>& bars) override;
        std::vector<StoredBar> readBars(const std::string& symbol, const Timeframe& tf, int64_t start, int64_t end) override;

        // 管理元数据
        void registerSymbol(const SymbolInfo& info);
        std::vector<SymbolInfo> getAllSymbols() const;

    private:
        sqlite3* db_;
        std::string db_path_;
        std::unique_ptr<FlatFileStore> file_store_;
        std::mutex db_mutex_;

        void initDB();
        void executeUpdate(const std::string& sql);
        // 更新最近时间戳缓存
        void updateLastTimestamp(const std::string& symbol, const Timeframe& tf, int64_t timestamp);
    };

} // namespace data
} // namespace bt
