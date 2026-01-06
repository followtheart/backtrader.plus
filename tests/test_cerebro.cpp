/**
 * @file test_cerebro.cpp
 * @brief Cerebro and Strategy unit tests
 */

#include <gtest/gtest.h>
#include "bt/backtrader.hpp"
#include <cmath>
#include <memory>

namespace {

// Simple test strategy that buys when price crosses above SMA
class TestSMAStrategy : public bt::Strategy {
public:
    void init() override {
        // We'll manually track SMA for simplicity
        setMinPeriod(smaPeriod_);
    }
    
    void next() override {
        if (!data(0)) return;
        
        // Simple moving average calculation
        auto& closePrice = data(0)->close();
        if (closePrice.length() < smaPeriod_) return;
        
        double sum = 0;
        for (bt::Size i = 0; i < smaPeriod_; ++i) {
            sum += closePrice[i];
        }
        double sma = sum / smaPeriod_;
        
        double price = closePrice[0];
        double prevPrice = closePrice.length() > 1 ? closePrice[1] : price;
        
        // Cross above SMA: buy
        if (prevPrice <= sma && price > sma && position() == 0) {
            buy();
        }
        // Cross below SMA: sell
        else if (prevPrice >= sma && price < sma && position() > 0) {
            closePosition();  // Close the position
        }
    }
    
private:
    bt::Size smaPeriod_ = 5;
};

// Simple buy and hold strategy
class BuyHoldStrategy : public bt::Strategy {
public:
    void nextstart() override {
        buy();  // Buy on first valid bar
    }
    
    void next() override {
        // Hold forever
    }
};

// Strategy that tracks its lifecycle
class LifecycleStrategy : public bt::Strategy {
public:
    bool initCalled = false;
    bool startCalled = false;
    bool stopCalled = false;
    int prenextCount = 0;
    int nextstartCount = 0;
    int nextCount = 0;
    
    void init() override { initCalled = true; }
    void start() override { startCalled = true; }
    void prenext() override { prenextCount++; }
    void nextstart() override { nextstartCount++; next(); }
    void next() override { nextCount++; }
    void stop() override { stopCalled = true; }
};

class CerebroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sample data
        sampleData_ = std::make_shared<bt::MemoryDataFeed>();
        
        // Add some test data - 20 bars of price data
        // Prices that will trigger some trades
        std::vector<double> prices = {
            100, 101, 102, 103, 104,   // Uptrend
            105, 106, 107, 108, 109,   // Continue up
            108, 107, 106, 105, 104,   // Downtrend
            103, 104, 105, 106, 107    // Recovery
        };
        
        for (size_t i = 0; i < prices.size(); ++i) {
            bt::DateTime dt(2024, 1, i + 1);
            double p = prices[i];
            sampleData_->addBar(dt, p, p + 1, p - 1, p, 1000, 0);
        }
    }
    
    std::shared_ptr<bt::MemoryDataFeed> sampleData_;
};

TEST_F(CerebroTest, BasicCreation) {
    bt::Cerebro cerebro;
    
    // Should have default broker
    EXPECT_EQ(cerebro.broker().getCash(), 100000.0);
}

TEST_F(CerebroTest, AddData) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_, "test_data");
    
    EXPECT_EQ(cerebro.dataCount(), 1);
    EXPECT_NE(cerebro.getData(0), nullptr);
}

TEST_F(CerebroTest, SetCash) {
    bt::Cerebro cerebro;
    cerebro.setCash(50000.0);
    
    EXPECT_EQ(cerebro.broker().getCash(), 50000.0);
}

TEST_F(CerebroTest, RunEmpty) {
    bt::Cerebro cerebro;
    
    // Running with no data should return empty results
    auto results = cerebro.run();
    EXPECT_TRUE(results.empty());
}

TEST_F(CerebroTest, RunWithData) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    
    auto results = cerebro.run();
    
    // Should have one result (default strategy)
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].totalBars, 20);
}

TEST_F(CerebroTest, StrategyLifecycle) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.addStrategy<LifecycleStrategy>();
    
    auto results = cerebro.run();
    
    // Get the strategy instance (stored in results)
    ASSERT_EQ(results.size(), 1);
    ASSERT_FALSE(results[0].strategies.empty());
    
    auto* strategy = dynamic_cast<LifecycleStrategy*>(results[0].strategies[0]);
    ASSERT_NE(strategy, nullptr);
    
    EXPECT_TRUE(strategy->startCalled);
    EXPECT_TRUE(strategy->stopCalled);
    EXPECT_GT(strategy->nextCount, 0);
}

TEST_F(CerebroTest, BuyHoldStrategy) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.setCash(10000.0);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto results = cerebro.run();
    
    ASSERT_EQ(results.size(), 1);
    
    // Should have made one trade
    EXPECT_GT(results[0].totalTrades, 0);
    
    // End value should be different from start (price changed)
    EXPECT_NE(results[0].endValue, results[0].startCash);
}

TEST_F(CerebroTest, RunResult) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.setCash(100000.0);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto results = cerebro.run();
    
    ASSERT_EQ(results.size(), 1);
    
    auto& result = results[0];
    EXPECT_EQ(result.startCash, 100000.0);
    EXPECT_EQ(result.totalBars, 20);
    
    // PnL calculation should be correct
    EXPECT_NEAR(result.pnl, result.endValue - result.startCash, 0.01);
}

// ==================== Analyzer Tests ====================

TEST_F(CerebroTest, DrawDownAnalyzer) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto* dd = cerebro.addAnalyzer<bt::DrawDown>();
    
    auto results = cerebro.run();
    
    auto analysis = dd->getAnalysis();
    
    // Should have drawdown metrics
    EXPECT_TRUE(analysis.count("max_drawdown") > 0);
    EXPECT_TRUE(analysis.count("max_moneydown") > 0);
}

TEST_F(CerebroTest, SharpeRatioAnalyzer) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto* sharpe = cerebro.addAnalyzer<bt::SharpeRatio>();
    
    cerebro.run();
    
    auto analysis = sharpe->getAnalysis();
    
    EXPECT_TRUE(analysis.count("sharpe_ratio") > 0);
}

// ==================== Observer Tests ====================

TEST_F(CerebroTest, CashObserver) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto* cashObs = cerebro.addObserver<bt::CashObserver>();
    
    cerebro.run();
    
    // Cash line should have values
    EXPECT_GT(cashObs->cash().length(), 0);
}

TEST_F(CerebroTest, ValueObserver) {
    bt::Cerebro cerebro;
    cerebro.addData(sampleData_);
    cerebro.addStrategy<BuyHoldStrategy>();
    
    auto* valueObs = cerebro.addObserver<bt::ValueObserver>();
    
    cerebro.run();
    
    // Value line should have values
    EXPECT_GT(valueObs->value().length(), 0);
}

// ==================== Broker Tests ====================

TEST(BrokerTest, BasicOperations) {
    bt::Broker broker(100000.0);
    
    EXPECT_EQ(broker.getCash(), 100000.0);
    EXPECT_EQ(broker.getStartCash(), 100000.0);
}

TEST(BrokerTest, Reset) {
    bt::Broker broker(100000.0);
    
    // Simulate some trading (would need data for real test)
    broker.reset();
    
    EXPECT_EQ(broker.getCash(), 100000.0);
}

// ==================== Strategy Tests ====================

TEST(StrategyTest, DataAccess) {
    bt::Strategy strategy;
    
    auto data = std::make_shared<bt::MemoryDataFeed>();
    bt::DateTime dt(2024, 1, 1);
    data->addBar(dt, 100, 101, 99, 100, 1000, 0);
    
    strategy.addData(data.get(), "test");
    
    EXPECT_EQ(strategy.dataCount(), 1);
    EXPECT_EQ(strategy.data(0), data.get());
    EXPECT_EQ(strategy.getDataName(0), "test");
}

TEST(StrategyTest, MinPeriod) {
    bt::Strategy strategy;
    
    EXPECT_EQ(strategy.minPeriod(), 1);
    
    strategy.setMinPeriod(10);
    EXPECT_EQ(strategy.minPeriod(), 10);
    
    strategy.updateMinPeriod(5);  // Should not change (5 < 10)
    EXPECT_EQ(strategy.minPeriod(), 10);
    
    strategy.updateMinPeriod(15);  // Should change (15 > 10)
    EXPECT_EQ(strategy.minPeriod(), 15);
}

// ==================== Order Tests ====================

TEST(OrderTest, Creation) {
    auto order = bt::Order::createMarket(100, 50.0);
    
    EXPECT_EQ(order.type(), bt::OrderType::Market);
    EXPECT_EQ(order.side(), bt::OrderSide::Buy);
    EXPECT_EQ(order.size(), 100);
}

TEST(OrderTest, LimitOrder) {
    auto order = bt::Order::createLimit(-50, 100.0);
    
    EXPECT_EQ(order.type(), bt::OrderType::Limit);
    EXPECT_EQ(order.side(), bt::OrderSide::Sell);
    EXPECT_EQ(order.size(), 50);
    EXPECT_EQ(order.price(), 100.0);
}

TEST(OrderTest, StatusChecks) {
    bt::Order order(1, bt::OrderSide::Buy, bt::OrderType::Market, 100);
    
    EXPECT_TRUE(order.isAlive());
    EXPECT_TRUE(order.isBuy());
    EXPECT_FALSE(order.isSell());
    
    order.setStatus(bt::OrderStatus::Completed);
    EXPECT_FALSE(order.isAlive());
}

// ==================== Trade Tests ====================

TEST(TradeTest, BasicTrade) {
    bt::Trade trade;
    trade.barOpen = 0;
    trade.priceOpen = 100.0;
    trade.size = 10;
    trade.isLong = true;
    trade.isOpen = true;
    
    EXPECT_TRUE(trade.isOpen);
    
    trade.close(10, 110.0, 1.0);
    
    EXPECT_FALSE(trade.isOpen);
    EXPECT_EQ(trade.barClose, 10);
    EXPECT_EQ(trade.priceClose, 110.0);
    EXPECT_NEAR(trade.pnl, 100.0, 0.01);  // (110 - 100) * 10
    EXPECT_NEAR(trade.pnlComm, 99.0, 0.01);  // 100 - 1 commission
}

} // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
