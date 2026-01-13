#include "bt/data/flat_file_store.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace bt {
namespace data {

    FlatFileStore::FlatFileStore(const std::string& baseDir) 
        : base_dir_(baseDir) {
        if (!fs::exists(base_dir_)) {
            fs::create_directories(base_dir_);
        }
    }

    fs::path FlatFileStore::getFilePath(const std::string& symbol, const Timeframe& tf) {
        // Structure: base_dir/symbol/timeframe.bin
        // Note: In production, sanitize symbol to remove illegal chars (e.g. windows specific)
        fs::path symbolPath = fs::path(base_dir_) / symbol;
        if (!fs::exists(symbolPath)) {
            fs::create_directories(symbolPath);
        }
        return symbolPath / (tf.toString() + ".bin");
    }

    int64_t FlatFileStore::getLastTimestamp(const std::string& symbol, const Timeframe& tf) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        
        fs::path path = getFilePath(symbol, tf);
        if (!fs::exists(path)) {
            return 0; // No data yet
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate); // Open at end
        if (!file.is_open()) return 0;

        std::streamsize size = file.tellg();
        if (size < static_cast<std::streamsize>(sizeof(StoredBar))) {
            return 0; // Empty file or corrupt
        }

        // Seek to last record
        file.seekg(-static_cast<std::streamsize>(sizeof(StoredBar)), std::ios::end);
        
        StoredBar bar;
        file.read(reinterpret_cast<char*>(&bar), sizeof(StoredBar));
        
        if (file.gcount() != sizeof(StoredBar)) return 0;

        return bar.timestamp;
    }

    void FlatFileStore::writeBars(const std::string& symbol, const Timeframe& tf, const std::vector<StoredBar>& bars) {
        if (bars.empty()) return;

        std::lock_guard<std::mutex> lock(file_mutex_);
        fs::path path = getFilePath(symbol, tf);

        // Append mode
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            // Try to create parent if somehow missing (handled in getFilePath but good to be safe)
             std::cerr << "Failed to open file for writing: " << path << std::endl;
             return;
        }

        file.write(reinterpret_cast<const char*>(bars.data()), bars.size() * sizeof(StoredBar));
    }

    std::vector<StoredBar> FlatFileStore::readBars(const std::string& symbol, const Timeframe& tf, int64_t start, int64_t end) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        fs::path path = getFilePath(symbol, tf);
        
        std::vector<StoredBar> result;
        if (!fs::exists(path)) return result;

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return result;

        // Simple Scan Implementation
        // TODO: Optimize with Binary Search on file offsets for large datasets
        
        // Improve buffer reading for performance
        const size_t BUFFER_SIZE = 1024; // Read 1024 bars at a time
        std::vector<StoredBar> buffer(BUFFER_SIZE);

        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE * sizeof(StoredBar));
            std::streamsize bytesRead = file.gcount();
            size_t barsRead = bytesRead / sizeof(StoredBar);

            for (size_t i = 0; i < barsRead; ++i) {
                const auto& bar = buffer[i];
                if (bar.timestamp >= start && bar.timestamp <= end) {
                    result.push_back(bar);
                } else if (bar.timestamp > end) {
                    // Assuming sorted data, we can stop early
                    return result; 
                }
            }

            if (bytesRead < static_cast<std::streamsize>(BUFFER_SIZE * sizeof(StoredBar))) {
                break; // EOF
            }
        }

        return result;
    }

} // namespace data
} // namespace bt
