/**
 * @file writer.hpp
 * @brief Writer System
 * 
 * Corresponds to Python's writer.py
 * Provides output functionality for backtest results.
 * 
 * Writers output data during and after backtesting:
 * - CSV output of trades, positions, equity
 * - Summary reports
 * - Real-time logging
 */

#pragma once

#include "bt/common.hpp"
#include "bt/params.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <cmath>

namespace bt {

// Forward declarations
class Strategy;
class Broker;
class DataFeed;
class Order;
struct Trade;

/**
 * @brief Base Writer class
 * 
 * Writers produce output during backtest execution.
 * They follow the same lifecycle as strategies and analyzers.
 */
class Writer : public Parametrized<Writer> {
public:
    virtual ~Writer() = default;
    
    // Lifecycle methods
    virtual void start() {}
    virtual void prenext() {}
    virtual void nextstart() { next(); }
    virtual void next() {}
    virtual void stop() {}
    
    // Setup
    void setStrategy(Strategy* s) { strategy_ = s; }
    void setBroker(Broker* b) { broker_ = b; }
    Strategy* strategy() const { return strategy_; }
    Broker* broker() const { return broker_; }

protected:
    Strategy* strategy_ = nullptr;
    Broker* broker_ = nullptr;
};

/**
 * @brief CSV File Writer
 * 
 * Outputs backtest data to CSV files.
 * 
 * Params:
 * - csv: Enable CSV output (default: true)
 * - csvsep: CSV separator (default: ',')
 * - csv_filternan: Filter out NaN values (default: true)
 * - csv_counter: Include row counter (default: true)
 * - out: Output file path (default: stdout)
 */
class WriterFile : public Writer {
public:
    struct Params {
        bool csv = true;
        char csvsep = ',';
        bool csv_filternan = true;
        bool csv_counter = true;
        std::string out;  // Empty = stdout
        bool close_out = false;
        int rounding = -1;  // Decimal places (-1 = no rounding)
        std::vector<std::string> separators = {"========================================"};
    };
    
    WriterFile() = default;
    explicit WriterFile(const std::string& filename) {
        params_.out = filename;
    }
    
    ~WriterFile() override {
        closeFile();
    }
    
    void start() override {
        if (!params_.out.empty()) {
            file_.open(params_.out);
            useFile_ = file_.is_open();
        }
        counter_ = 0;
    }
    
    void stop() override {
        // Write final separator
        writeSeparator();
        closeFile();
    }
    
    /**
     * @brief Write headers to output
     */
    void writeHeaders(const std::vector<std::string>& headers) {
        headers_ = headers;
        
        std::ostringstream oss;
        
        if (params_.csv_counter) {
            oss << "Index" << params_.csvsep;
        }
        
        for (size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) oss << params_.csvsep;
            oss << headers[i];
        }
        
        writeLine(oss.str());
    }
    
    /**
     * @brief Write a line of values
     */
    void writeValues(const std::vector<Value>& values) {
        std::ostringstream oss;
        
        if (params_.csv_counter) {
            oss << counter_++ << params_.csvsep;
        }
        
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) oss << params_.csvsep;
            writeValue(oss, values[i]);
        }
        
        writeLine(oss.str());
    }
    
    /**
     * @brief Write a map of key-value pairs
     */
    void writeDict(const std::map<std::string, Value>& dict) {
        for (const auto& [key, value] : dict) {
            std::ostringstream oss;
            oss << key << params_.csvsep;
            writeValue(oss, value);
            writeLine(oss.str());
        }
    }
    
    /**
     * @brief Write a separator line
     */
    void writeSeparator(size_t idx = 0) {
        if (idx < params_.separators.size()) {
            writeLine(params_.separators[idx]);
        }
    }
    
    /**
     * @brief Write a raw line
     */
    void writeLine(const std::string& line) {
        if (useFile_) {
            file_ << line << "\n";
        } else {
            std::cout << line << "\n";
        }
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    void writeValue(std::ostringstream& oss, Value v) {
        if (params_.csv_filternan && std::isnan(v)) {
            oss << "";
            return;
        }
        
        if (params_.rounding >= 0) {
            oss << std::fixed << std::setprecision(params_.rounding) << v;
        } else {
            oss << v;
        }
    }
    
    void closeFile() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    Params params_;
    std::ofstream file_;
    bool useFile_ = false;
    std::vector<std::string> headers_;
    size_t counter_ = 0;
};

/**
 * @brief Trade Writer
 * 
 * Specialized writer for trade output.
 * Writes trade details as they occur.
 */
class TradeWriter : public WriterFile {
public:
    TradeWriter() {
        // Default headers for trade output
        headers_ = {"DateTime", "Symbol", "Side", "Size", "Price", 
                    "Commission", "PnL", "PnLComm", "PositionSize"};
    }
    
    explicit TradeWriter(const std::string& filename) : WriterFile(filename) {
        headers_ = {"DateTime", "Symbol", "Side", "Size", "Price",
                    "Commission", "PnL", "PnLComm", "PositionSize"};
    }
    
    void start() override {
        WriterFile::start();
        writeHeaders(headers_);
    }
    
    /**
     * @brief Write a trade record
     */
    void writeTrade(const std::string& datetime, const std::string& symbol,
                    const std::string& side, Value size, Value price,
                    Value commission, Value pnl, Value pnlComm, Value posSize) {
        std::ostringstream oss;
        
        if (params().csv_counter) {
            static size_t counter = 0;
            oss << counter++ << params().csvsep;
        }
        
        oss << datetime << params().csvsep
            << symbol << params().csvsep
            << side << params().csvsep
            << size << params().csvsep
            << price << params().csvsep
            << commission << params().csvsep
            << pnl << params().csvsep
            << pnlComm << params().csvsep
            << posSize;
        
        writeLine(oss.str());
    }

private:
    std::vector<std::string> headers_;
};

/**
 * @brief Equity Writer
 * 
 * Writes equity curve data over time.
 */
class EquityWriter : public WriterFile {
public:
    EquityWriter() {
        headers_ = {"DateTime", "Cash", "Value", "Return", "DrawDown"};
    }
    
    explicit EquityWriter(const std::string& filename) : WriterFile(filename) {
        headers_ = {"DateTime", "Cash", "Value", "Return", "DrawDown"};
    }
    
    void start() override {
        WriterFile::start();
        writeHeaders(headers_);
        prevValue_ = 0;
        maxValue_ = 0;
    }
    
    /**
     * @brief Write an equity record
     */
    void writeEquity(const std::string& datetime, Value cash, Value value) {
        // Calculate return
        Value ret = 0;
        if (prevValue_ > 0) {
            ret = (value - prevValue_) / prevValue_ * 100.0;
        }
        
        // Calculate drawdown
        if (value > maxValue_) {
            maxValue_ = value;
        }
        Value dd = 0;
        if (maxValue_ > 0) {
            dd = (maxValue_ - value) / maxValue_ * 100.0;
        }
        
        writeValues({cash, value, ret, dd});
        
        // Add datetime prefix
        prevValue_ = value;
    }

private:
    std::vector<std::string> headers_;
    Value prevValue_ = 0;
    Value maxValue_ = 0;
};

/**
 * @brief Order Writer
 * 
 * Writes order details as they are processed.
 */
class OrderWriter : public WriterFile {
public:
    OrderWriter() {
        headers_ = {"DateTime", "Ref", "Type", "Status", "Side", 
                    "Size", "Price", "ExecPrice", "Commission"};
    }
    
    explicit OrderWriter(const std::string& filename) : WriterFile(filename) {
        headers_ = {"DateTime", "Ref", "Type", "Status", "Side",
                    "Size", "Price", "ExecPrice", "Commission"};
    }
    
    void start() override {
        WriterFile::start();
        writeHeaders(headers_);
    }
    
    /**
     * @brief Write an order record
     */
    void writeOrder(const std::string& datetime, int ref,
                    const std::string& type, const std::string& status,
                    const std::string& side, Value size, Value price,
                    Value execPrice, Value commission) {
        std::ostringstream oss;
        
        if (params().csv_counter) {
            static size_t counter = 0;
            oss << counter++ << params().csvsep;
        }
        
        oss << datetime << params().csvsep
            << ref << params().csvsep
            << type << params().csvsep
            << status << params().csvsep
            << side << params().csvsep
            << size << params().csvsep
            << price << params().csvsep
            << execPrice << params().csvsep
            << commission;
        
        writeLine(oss.str());
    }

private:
    std::vector<std::string> headers_;
};

/**
 * @brief Summary Writer
 * 
 * Writes a summary report at the end of backtesting.
 */
class SummaryWriter : public Writer {
public:
    struct Params {
        std::string out;  // Empty = stdout
        int indent = 2;
    };
    
    SummaryWriter() = default;
    explicit SummaryWriter(const std::string& filename) {
        params_.out = filename;
    }
    
    ~SummaryWriter() override {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void start() override {
        if (!params_.out.empty()) {
            file_.open(params_.out);
            useFile_ = file_.is_open();
        }
    }
    
    void stop() override {
        // Write summary
        writeSummary();
        
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    /**
     * @brief Add a section to the summary
     */
    void addSection(const std::string& title, 
                    const std::map<std::string, Value>& data) {
        sections_.push_back({title, data});
    }
    
    /**
     * @brief Add a text note to the summary
     */
    void addNote(const std::string& note) {
        notes_.push_back(note);
    }
    
    Params& params() { return params_; }
    const Params& params() const { return params_; }

private:
    void writeSummary() {
        std::ostream& out = useFile_ ? file_ : std::cout;
        std::string indent(params_.indent, ' ');
        
        out << "========================================\n";
        out << "           BACKTEST SUMMARY\n";
        out << "========================================\n\n";
        
        for (const auto& [title, data] : sections_) {
            out << title << ":\n";
            out << "----------------------------------------\n";
            for (const auto& [key, value] : data) {
                out << indent << std::setw(20) << std::left << key << ": ";
                out << std::fixed << std::setprecision(2) << value << "\n";
            }
            out << "\n";
        }
        
        if (!notes_.empty()) {
            out << "Notes:\n";
            out << "----------------------------------------\n";
            for (const auto& note : notes_) {
                out << indent << "- " << note << "\n";
            }
        }
        
        out << "========================================\n";
    }
    
    Params params_;
    std::ofstream file_;
    bool useFile_ = false;
    std::vector<std::pair<std::string, std::map<std::string, Value>>> sections_;
    std::vector<std::string> notes_;
};

/**
 * @brief Multi-Writer - combines multiple writers
 * 
 * Allows running multiple writers simultaneously.
 */
class MultiWriter : public Writer {
public:
    void addWriter(std::unique_ptr<Writer> writer) {
        writers_.push_back(std::move(writer));
    }
    
    void start() override {
        for (auto& w : writers_) {
            w->setStrategy(strategy());
            w->setBroker(broker());
            w->start();
        }
    }
    
    void prenext() override {
        for (auto& w : writers_) {
            w->prenext();
        }
    }
    
    void nextstart() override {
        for (auto& w : writers_) {
            w->nextstart();
        }
    }
    
    void next() override {
        for (auto& w : writers_) {
            w->next();
        }
    }
    
    void stop() override {
        for (auto& w : writers_) {
            w->stop();
        }
    }

private:
    std::vector<std::unique_ptr<Writer>> writers_;
};

/**
 * @brief Stream Writer - write to any ostream
 * 
 * Generic writer that outputs to any std::ostream.
 */
class StreamWriter : public Writer {
public:
    explicit StreamWriter(std::ostream& stream) : stream_(stream) {}
    
    void writeLine(const std::string& line) {
        stream_ << line << "\n";
    }
    
    template<typename... Args>
    void write(Args&&... args) {
        (stream_ << ... << std::forward<Args>(args));
    }
    
    void flush() {
        stream_.flush();
    }

private:
    std::ostream& stream_;
};

} // namespace bt
