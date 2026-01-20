/**
 * @file backtest_with_stored_data.cpp
 * @brief Example: Backtest using data stored by fetch_and_store
 * 
 * This example demonstrates:
 * 1. Reading historical bar data from SQLiteStore
 * 2. Converting data to backtesting engine format
 * 3. Running SMA crossover strategy backtest
 * 
 * Run fetch_and_store first to download data
 */

#include <iostream>
#include <iomanip>
#include <memory>
#include <ctime>

#include "bt/backtrader.hpp"
#include "bt/data/sqlite_store.hpp"
#include "bt/data/types.hpp"

using namespace bt;
using namespace bt::data;

/**
 * @brief DataFeed that loads data from SQLiteStore
 */
class SQLiteDataFeed : public DataFeed {
public:
    SQLiteDataFeed(std::shared_ptr<SQLiteStore> store,
                   const std::string& symbol,
                   const Timeframe& tf,
                   int64_t startTs = 0,
                   int64_t endTs = 0)
        : store_(store), symbol_(symbol), tf_(tf), startTs_(startTs), endTs_(endTs) {}

    bool load() override {
        if (endTs_ == 0) {
            endTs_ = store_->getLastTimestamp(symbol_, tf_);
        }
        if (startTs_ == 0) {
            startTs_ = endTs_ - (30LL * 24 * 60 * 60 * 1000);
        }

        if (endTs_ <= 0) {
            std::cerr << "No data found for " << symbol_ << " " << tf_.toString() << std::endl;
            return false;
        }

        std::cout << "Loading data from " << timestampToString(startTs_) 
                  << " to " << timestampToString(endTs_) << std::endl;

        auto bars = store_->readBars(symbol_, tf_, startTs_, endTs_);
        
        if (bars.empty()) {
            std::cerr << "No bars loaded!" << std::endl;
            return false;
        }

        std::cout << "Loaded " << bars.size() << " bars" << std::endl;

        for (const auto& bar : bars) {
            double dtValue = static_cast<double>(bar.timestamp / 1000) / 86400.0;
            OHLCVData::addBar(bar.open, bar.high, bar.low, bar.close, 
                              bar.volume, bar.open_interest);
            datetime().push(dtValue);
        }

        return length() > 0;
    }

private:
    std::shared_ptr<SQLiteStore> store_;
    std::string symbol_;
    Timeframe tf_;
    int64_t startTs_;
    int64_t endTs_;

    static std::string timestampToString(int64_t ts) {
        time_t t = ts / 1000;
        std::tm* tm = std::localtime(&t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return std::string(buf);
    }
};

/**
 * @brief SMA Crossover Strategy
 */
class SMACrossStrategy : public Strategy {
public:
    SMACrossStrategy(Size fastPeriod = 10, Size slowPeriod = 30)
        : fastPeriod_(fastPeriod), slowPeriod_(slowPeriod) {}

    void init() override {
        setMinPeriod(slowPeriod_);
        std::cout << "Strategy initialized: Fast SMA=" << fastPeriod_ 
                  << ", Slow SMA=" << slowPeriod_ << std::endl;
    }

    void next() override {
        if (!data(0)) return;

        auto& closePrice = data(0)->close();
        if (closePrice.length() < slowPeriod_) return;

        double fastSMA = calculateSMA(closePrice, fastPeriod_);
        double slowSMA = calculateSMA(closePrice, slowPeriod_);
        double prevFastSMA = calculateSMA(closePrice, fastPeriod_, 1);
        double prevSlowSMA = calculateSMA(closePrice, slowPeriod_, 1);

        bool goldenCross = prevFastSMA <= prevSlowSMA && fastSMA > slowSMA;
        bool deathCross = prevFastSMA >= prevSlowSMA && fastSMA < slowSMA;

        Value pos = position();
        double currentPrice = closePrice[0];

        if (goldenCross && pos <= 0) {
            if (pos < 0) closePosition();
            buy();
            tradeCount_++;
            std::cout << "[" << tradeCount_ << "] BUY  @ " << std::fixed << std::setprecision(2)
                      << currentPrice << " | FastSMA: " << fastSMA
                      << " > SlowSMA: " << slowSMA << std::endl;
        }
        else if (deathCross && pos >= 0) {
            if (pos > 0) closePosition();
            sell();
            tradeCount_++;
            std::cout << "[" << tradeCount_ << "] SELL @ " << std::fixed << std::setprecision(2)
                      << currentPrice << " | FastSMA: " << fastSMA
                      << " < SlowSMA: " << slowSMA << std::endl;
        }
    }

    void notifyTrade(Trade& trade) override {
        if (!trade.isOpen) {
            totalPnl_ += trade.pnlComm;
            std::cout << "    -> Trade Closed | PnL: " << std::fixed << std::setprecision(2)
                      << trade.pnlComm << " | Total PnL: " << totalPnl_ << std::endl;
        }
    }

private:
    Size fastPeriod_;
    Size slowPeriod_;
    int tradeCount_ = 0;
    double totalPnl_ = 0.0;

    double calculateSMA(const LineBuffer& line, Size period, Size offset = 0) {
        if (line.length() < period + offset) return 0;
        double sum = 0;
        for (Size j = offset; j < period + offset; ++j) {
            sum += line[j];
        }
        return sum / period;
    }
};

/**
 * @brief RSI Overbought/Oversold Strategy
 */
class RSIStrategy : public Strategy {
public:
    RSIStrategy(Size period = 14, double oversold = 30.0, double overbought = 70.0)
        : period_(period), oversold_(oversold), overbought_(overbought) {}

    void init() override {
        setMinPeriod(period_ + 1);
        std::cout << "RSI Strategy: period=" << period_ 
                  << ", oversold=" << oversold_ 
                  << ", overbought=" << overbought_ << std::endl;
    }

    void next() override {
        if (!data(0)) return;

        auto& closePrice = data(0)->close();
        if (closePrice.length() < period_ + 1) return;

        double rsi = calculateRSI(closePrice, period_);
        double prevRsi = calculateRSI(closePrice, period_, 1);
        
        Value pos = position();
        double currentPrice = closePrice[0];

        if (prevRsi < oversold_ && rsi >= oversold_ && pos <= 0) {
            if (pos < 0) closePosition();
            buy();
            std::cout << "BUY  @ " << std::fixed << std::setprecision(2)
                      << currentPrice << " | RSI: " << rsi << " (exit oversold)" << std::endl;
        }
        else if (prevRsi > overbought_ && rsi <= overbought_ && pos >= 0) {
            if (pos > 0) closePosition();
            sell();
            std::cout << "SELL @ " << std::fixed << std::setprecision(2)
                      << currentPrice << " | RSI: " << rsi << " (exit overbought)" << std::endl;
        }
    }

private:
    Size period_;
    double oversold_;
    double overbought_;

    double calculateRSI(const LineBuffer& line, Size period, Size offset = 0) {
        if (line.length() < period + 1 + offset) return 50.0;

        double gains = 0;
        double losses = 0;
        for (Size j = offset; j < period + offset; ++j) {
            double change = line[j] - line[j + 1];
            if (change > 0) {
                gains += change;
            } else {
                losses -= change;
            }
        }

        if (losses == 0) return 100.0;
        double rs = gains / losses;
        return 100.0 - (100.0 / (1.0 + rs));
    }
};

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  --symbol <sym>     Symbol to backtest (default: BTC-USDT)\n"
              << "  --timeframe <tf>   Timeframe: 1m, 5m, 15m, 1h, 4h, 1d (default: 1m)\n"
              << "  --days <n>         Number of days to backtest (default: 7)\n"
              << "  --strategy <s>     Strategy: sma, rsi (default: sma)\n"
              << "  --cash <amount>    Initial cash (default: 100000)\n"
              << "  --db <path>        Database path (default: data/metadata.db)\n"
              << "  --datadir <path>   Data directory (default: data/bars)\n"
              << std::endl;
}

Timeframe parseTimeframe(const std::string& tfInput) {
    if (tfInput == "1m")  return {Timeframe::Minute, 1};
    if (tfInput == "5m")  return {Timeframe::Minute, 5};
    if (tfInput == "15m") return {Timeframe::Minute, 15};
    if (tfInput == "30m") return {Timeframe::Minute, 30};
    if (tfInput == "1h")  return {Timeframe::Hour, 1};
    if (tfInput == "4h")  return {Timeframe::Hour, 4};
    if (tfInput == "1d")  return {Timeframe::Day, 1};
    return {Timeframe::Minute, 1};
}

int main(int argc, char* argv[]) {
    std::string symbol = "BTC-USDT";
    std::string tfStr = "1m";
    int days = 7;
    std::string strategyType = "sma";
    double initialCash = 100000.0;
    std::string dbPath = "data/metadata.db";
    std::string dataDir = "data/bars";

    for (int idx = 1; idx < argc; ++idx) {
        std::string arg = argv[idx];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--symbol" && idx + 1 < argc) {
            symbol = argv[++idx];
        }
        else if (arg == "--timeframe" && idx + 1 < argc) {
            tfStr = argv[++idx];
        }
        else if (arg == "--days" && idx + 1 < argc) {
            days = std::stoi(argv[++idx]);
        }
        else if (arg == "--strategy" && idx + 1 < argc) {
            strategyType = argv[++idx];
        }
        else if (arg == "--cash" && idx + 1 < argc) {
            initialCash = std::stod(argv[++idx]);
        }
        else if (arg == "--db" && idx + 1 < argc) {
            dbPath = argv[++idx];
        }
        else if (arg == "--datadir" && idx + 1 < argc) {
            dataDir = argv[++idx];
        }
    }

    Timeframe tf = parseTimeframe(tfStr);

    std::cout << "========================================" << std::endl;
    std::cout << "  Backtest with Stored Data" << std::endl;
    std::cout << "  Backtrader C++ " << version() << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Symbol:     " << symbol << std::endl;
    std::cout << "Timeframe:  " << tf.toString() << std::endl;
    std::cout << "Days:       " << days << std::endl;
    std::cout << "Strategy:   " << strategyType << std::endl;
    std::cout << "Cash:       " << initialCash << std::endl;
    std::cout << "Database:   " << dbPath << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    try {
        auto store = std::make_shared<SQLiteStore>(dbPath, dataDir);

        int64_t endTs = store->getLastTimestamp(symbol, tf);
        if (endTs <= 0) {
            std::cerr << "Error: No data found for " << symbol << " " << tf.toString() << std::endl;
            std::cerr << "Please run fetch_and_store first to download data." << std::endl;
            return 1;
        }

        int64_t startTs = endTs - (static_cast<int64_t>(days) * 24 * 60 * 60 * 1000);

        auto dataFeed = std::make_shared<SQLiteDataFeed>(store, symbol, tf, startTs, endTs);
        if (!dataFeed->load()) {
            std::cerr << "Failed to load data!" << std::endl;
            return 1;
        }

        Cerebro cerebro;
        cerebro.addData(dataFeed, symbol);
        cerebro.setCash(initialCash);
        cerebro.broker().setCommission(std::make_shared<CommInfoStock>(0.001));

        if (strategyType == "rsi") {
            cerebro.addStrategy<RSIStrategy>(14, 30.0, 70.0);
        } else {
            cerebro.addStrategy<SMACrossStrategy>(10, 30);
        }

        auto* sharpe = cerebro.addAnalyzer<SharpeRatio>();
        auto* drawdown = cerebro.addAnalyzer<DrawDown>();

        std::cout << "Starting Backtest..." << std::endl;
        std::cout << "Initial Cash: $" << std::fixed << std::setprecision(2) 
                  << cerebro.broker().getCash() << std::endl;
        std::cout << std::endl;

        auto results = cerebro.run();

        std::cout << std::endl;
        std::cout << "========== Backtest Complete ==========" << std::endl;
        std::cout << std::endl;

        if (!results.empty()) {
            auto& result = results[0];

            std::cout << "========== RESULTS ==========" << std::endl;
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Initial Value:  $" << initialCash << std::endl;
            std::cout << "Final Value:    $" << result.endValue << std::endl;
            std::cout << "Total Return:   " << result.pnlPct << "%" << std::endl;
            std::cout << "Total Trades:   " << result.totalTrades << std::endl;

            auto sharpeAnalysis = sharpe->getAnalysis();
            if (sharpeAnalysis.count("sharpe_ratio")) {
                std::cout << "Sharpe Ratio:   " << sharpeAnalysis["sharpe_ratio"] << std::endl;
            }

            auto ddAnalysis = drawdown->getAnalysis();
            if (ddAnalysis.count("max_drawdown")) {
                std::cout << "Max Drawdown:   " << ddAnalysis["max_drawdown"] << "%" << std::endl;
            }

            std::cout << "=============================" << std::endl;

            double profit = result.endValue - initialCash;
            double profitPct = (profit / initialCash) * 100.0;
            
            std::cout << std::endl;
            std::cout << "Summary:" << std::endl;
            std::cout << "  Profit/Loss: $" << profit << " (" << profitPct << "%)" << std::endl;
            if (result.totalTrades > 0) {
                std::cout << "  Avg Trade:   $" << (profit / result.totalTrades) << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
