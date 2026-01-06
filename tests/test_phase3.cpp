/**
 * @file test_phase3.cpp
 * @brief Phase 3 feature tests - TimeFrame, Signal, Resampler
 */

#include <gtest/gtest.h>
#include "bt/timeframe.hpp"
#include "bt/signal.hpp"
#include "bt/signalstrategy.hpp"
#include "bt/resampler.hpp"
#include "bt/linebuffer.hpp"

using namespace bt;

// ==================== TimeFrame Tests ====================

class TimeFrameTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(TimeFrameTest, TimeFrameName) {
    EXPECT_EQ(timeframe::name(TimeFrame::Ticks), "Ticks");
    EXPECT_EQ(timeframe::name(TimeFrame::Seconds), "Seconds");
    EXPECT_EQ(timeframe::name(TimeFrame::Minutes), "Minutes");
    EXPECT_EQ(timeframe::name(TimeFrame::Days), "Days");
    EXPECT_EQ(timeframe::name(TimeFrame::Weeks), "Weeks");
    EXPECT_EQ(timeframe::name(TimeFrame::Months), "Months");
    EXPECT_EQ(timeframe::name(TimeFrame::Years), "Years");
}

TEST_F(TimeFrameTest, TimeFrameShortName) {
    EXPECT_EQ(timeframe::shortName(TimeFrame::Minutes), "M");
    EXPECT_EQ(timeframe::shortName(TimeFrame::Days), "D");
    EXPECT_EQ(timeframe::shortName(TimeFrame::Weeks), "W");
}

TEST_F(TimeFrameTest, TimeFrameParse) {
    EXPECT_EQ(timeframe::parse("Minutes"), TimeFrame::Minutes);
    EXPECT_EQ(timeframe::parse("min"), TimeFrame::Minutes);
    EXPECT_EQ(timeframe::parse("Days"), TimeFrame::Days);
    EXPECT_EQ(timeframe::parse("daily"), TimeFrame::Days);
    EXPECT_EQ(timeframe::parse("unknown"), TimeFrame::NoTimeFrame);
}

TEST_F(TimeFrameTest, TimeFrameComparison) {
    EXPECT_TRUE(timeframe::isSmaller(TimeFrame::Minutes, TimeFrame::Days));
    EXPECT_FALSE(timeframe::isSmaller(TimeFrame::Days, TimeFrame::Minutes));
    EXPECT_TRUE(timeframe::isLarger(TimeFrame::Weeks, TimeFrame::Days));
    EXPECT_TRUE(timeframe::isIntraday(TimeFrame::Minutes));
    EXPECT_FALSE(timeframe::isIntraday(TimeFrame::Days));
}

TEST_F(TimeFrameTest, TimeFrameDuration) {
    EXPECT_EQ(timeframe::durationSeconds(TimeFrame::Seconds, 1), 1);
    EXPECT_EQ(timeframe::durationSeconds(TimeFrame::Minutes, 5), 300);
    EXPECT_EQ(timeframe::durationSeconds(TimeFrame::Days, 1), 86400);
}

TEST_F(TimeFrameTest, TimeFrameConfig) {
    TimeFrameConfig config(TimeFrame::Minutes, 5);
    EXPECT_EQ(config.timeframe, TimeFrame::Minutes);
    EXPECT_EQ(config.compression, 5);
    EXPECT_EQ(config.shortString(), "5M");
    EXPECT_EQ(config.toString(), "5 Minutes");
    
    TimeFrameConfig config2(TimeFrame::Days, 1);
    EXPECT_TRUE(config < config2);  // Minutes < Days
}

// ==================== Signal Tests ====================

class SignalTest : public ::testing::Test {
protected:
    LineBuffer signalLine_;
    
    void SetUp() override {
        // Initialize with some data
        for (int i = 0; i < 10; ++i) {
            signalLine_.push(0.0);
        }
    }
};

TEST_F(SignalTest, SignalTypeName) {
    EXPECT_EQ(signal_utils::name(SignalType::SIGNAL_NONE), "SIGNAL_NONE");
    EXPECT_EQ(signal_utils::name(SignalType::SIGNAL_LONGSHORT), "SIGNAL_LONGSHORT");
    EXPECT_EQ(signal_utils::name(SignalType::SIGNAL_LONG), "SIGNAL_LONG");
    EXPECT_EQ(signal_utils::name(SignalType::SIGNAL_SHORT), "SIGNAL_SHORT");
}

TEST_F(SignalTest, SignalTypeChecks) {
    EXPECT_TRUE(signal_utils::isLongEntry(SignalType::SIGNAL_LONG));
    EXPECT_TRUE(signal_utils::isLongEntry(SignalType::SIGNAL_LONGSHORT));
    EXPECT_FALSE(signal_utils::isLongEntry(SignalType::SIGNAL_SHORT));
    
    EXPECT_TRUE(signal_utils::isShortEntry(SignalType::SIGNAL_SHORT));
    EXPECT_TRUE(signal_utils::isShortEntry(SignalType::SIGNAL_LONGSHORT));
    EXPECT_FALSE(signal_utils::isShortEntry(SignalType::SIGNAL_LONG));
    
    EXPECT_TRUE(signal_utils::isLongExit(SignalType::SIGNAL_LONGEXIT));
    EXPECT_TRUE(signal_utils::isShortExit(SignalType::SIGNAL_SHORTEXIT));
}

TEST_F(SignalTest, SignalEvaluation) {
    // SIGNAL_LONGSHORT: positive = long, negative = short
    EXPECT_EQ(signal_utils::evaluate(1.0, SignalType::SIGNAL_LONGSHORT), 1);
    EXPECT_EQ(signal_utils::evaluate(-1.0, SignalType::SIGNAL_LONGSHORT), -1);
    EXPECT_EQ(signal_utils::evaluate(0.0, SignalType::SIGNAL_LONGSHORT), 0);
    
    // SIGNAL_LONG: positive = long
    EXPECT_EQ(signal_utils::evaluate(1.0, SignalType::SIGNAL_LONG), 1);
    EXPECT_EQ(signal_utils::evaluate(-1.0, SignalType::SIGNAL_LONG), 0);
    
    // SIGNAL_LONG_INV: negative = long (inverted)
    EXPECT_EQ(signal_utils::evaluate(-1.0, SignalType::SIGNAL_LONG_INV), 1);
    EXPECT_EQ(signal_utils::evaluate(1.0, SignalType::SIGNAL_LONG_INV), 0);
    
    // SIGNAL_SHORT: negative = short
    EXPECT_EQ(signal_utils::evaluate(-1.0, SignalType::SIGNAL_SHORT), -1);
    EXPECT_EQ(signal_utils::evaluate(1.0, SignalType::SIGNAL_SHORT), 0);
}

TEST_F(SignalTest, SignalClass) {
    // Set current value using index [0] after advancing
    signalLine_[0] = 1.5;  // Positive signal
    
    Signal signal(&signalLine_, SignalType::SIGNAL_LONGSHORT);
    
    EXPECT_EQ(signal.signalType(), SignalType::SIGNAL_LONGSHORT);
    EXPECT_EQ(signal.signalTypeName(), "SIGNAL_LONGSHORT");
    EXPECT_DOUBLE_EQ(signal.value(), 1.5);
    EXPECT_EQ(signal.evaluate(), 1);  // Long signal
    EXPECT_TRUE(signal.isLong());
    EXPECT_FALSE(signal.isShort());
}

TEST_F(SignalTest, SignalGroup) {
    signalLine_[0] = 1.0;  // Positive signal
    
    Signal signal1(&signalLine_, SignalType::SIGNAL_LONG);
    Signal signal2(&signalLine_, SignalType::SIGNAL_LONGEXIT);
    
    SignalGroup group;
    group.addSignal(&signal1, SignalType::SIGNAL_LONG, 0);
    
    EXPECT_EQ(group.size(), 1u);
    EXPECT_FALSE(group.empty());
    EXPECT_TRUE(group.hasLongEntry());
    EXPECT_FALSE(group.hasShortEntry());
    
    // Add exit signal (negative value needed to trigger)
    signalLine_[0] = -1.0;
    group.addSignal(&signal2, SignalType::SIGNAL_LONGEXIT, 0);
    
    EXPECT_EQ(group.size(), 2u);
}

// ==================== Order Extensions Tests ====================

class OrderExtTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(OrderExtTest, OrderExecutionBit) {
    OrderExecutionBit bit(12345.0, 100, 50.5, 50, 2525.0, 5.0,
                          50, 2525.0, 5.0, 100.0, 100, 50.5);
    
    EXPECT_DOUBLE_EQ(bit.dt, 12345.0);
    EXPECT_DOUBLE_EQ(bit.size, 100);
    EXPECT_DOUBLE_EQ(bit.price, 50.5);
    EXPECT_DOUBLE_EQ(bit.closed, 50);
    EXPECT_DOUBLE_EQ(bit.opened, 50);
    EXPECT_DOUBLE_EQ(bit.pnl, 100.0);
}

TEST_F(OrderExtTest, OrderData) {
    OrderData data;
    data.remsize = 100;
    
    // Add first execution
    data.add(12345.0, 50, 100.0, 0, 0, 0, 50, 5000, 10, 0, 50, 100.0);
    
    EXPECT_DOUBLE_EQ(data.size, 50);
    EXPECT_DOUBLE_EQ(data.price, 100.0);
    EXPECT_EQ(data.exbits.size(), 1u);
    EXPECT_DOUBLE_EQ(data.opened, 50);
    
    // Add second execution
    data.add(12346.0, 50, 102.0, 0, 0, 0, 50, 5100, 10, 0, 100, 101.0);
    
    EXPECT_DOUBLE_EQ(data.size, 100);
    // Average price: (50*100 + 50*102) / 100 = 101
    EXPECT_NEAR(data.price, 101.0, 0.01);
    EXPECT_EQ(data.exbits.size(), 2u);
}

TEST_F(OrderExtTest, OrderTrailAdjust) {
    Order order = Order::createStopTrail(100, 0.0);
    order.setTrailAmount(5.0);
    
    // Simulate price movement for buy trailing stop
    // For buy: stop is above price
    order.trailAdjust(100.0);
    EXPECT_DOUBLE_EQ(order.created().price, 105.0);
    
    // Price goes down - stop should follow down
    order.trailAdjust(95.0);
    EXPECT_DOUBLE_EQ(order.created().price, 100.0);  // 95 + 5
}

TEST_F(OrderExtTest, OrderTypes) {
    // Test all order creation methods
    Order market = Order::createMarket(100);
    EXPECT_EQ(market.type(), OrderType::Market);
    EXPECT_EQ(market.side(), OrderSide::Buy);
    
    // Close order - for selling, we need to explicitly set the side
    Order close = Order::createClose(50);
    close.setSide(OrderSide::Sell);
    EXPECT_EQ(close.type(), OrderType::Close);
    EXPECT_EQ(close.side(), OrderSide::Sell);
    
    Order limit = Order::createLimit(100, 50.0);
    EXPECT_EQ(limit.type(), OrderType::Limit);
    EXPECT_DOUBLE_EQ(limit.price(), 50.0);
    
    // StopTrail sell order
    Order stopTrail = Order::createStopTrail(100, 2.0, 0.01);
    stopTrail.setSide(OrderSide::Sell);
    EXPECT_EQ(stopTrail.type(), OrderType::StopTrail);
    EXPECT_EQ(stopTrail.side(), OrderSide::Sell);
    EXPECT_DOUBLE_EQ(stopTrail.trailAmount(), 2.0);
    EXPECT_DOUBLE_EQ(stopTrail.trailPercent(), 0.01);
}

TEST_F(OrderExtTest, OrderOCOBracket) {
    Order main;
    Order stop;
    Order limit;
    
    // Setup bracket
    stop.setParent(&main);
    limit.setParent(&main);
    main.addChild(&stop);
    main.addChild(&limit);
    
    // Setup OCO between stop and limit
    stop.setOco(&limit);
    limit.setOco(&stop);
    
    EXPECT_EQ(stop.parent(), &main);
    EXPECT_EQ(limit.parent(), &main);
    EXPECT_EQ(main.children().size(), 2u);
    EXPECT_EQ(stop.oco(), &limit);
    EXPECT_EQ(limit.oco(), &stop);
}

TEST_F(OrderExtTest, OrderValidUntil) {
    Order order = Order::createLimit(100, 50.0);
    order.setValidUntil(12350.0);  // Valid until this datetime
    
    EXPECT_TRUE(order.hasValidUntil());
    EXPECT_DOUBLE_EQ(order.validUntil(), 12350.0);
    
    // Test expiration
    EXPECT_FALSE(order.expire(12340.0));  // Not expired yet
    EXPECT_TRUE(order.expire(12360.0));   // Now expired
    EXPECT_EQ(order.status(), OrderStatus::Expired);
}

// ==================== Resampler Tests ====================

class ResamplerTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ResamplerTest, OHLCVBar) {
    OHLCVBar bar;
    EXPECT_FALSE(bar.isOpen());
    
    bar.start(1.0, 100.0, 102.0, 99.0, 101.0, 1000, 0);
    EXPECT_TRUE(bar.isOpen());
    EXPECT_DOUBLE_EQ(bar.open, 100.0);
    EXPECT_DOUBLE_EQ(bar.high, 102.0);
    EXPECT_DOUBLE_EQ(bar.low, 99.0);
    EXPECT_DOUBLE_EQ(bar.close, 101.0);
    
    // Update with new tick
    bar.update(1.1, 101.0, 105.0, 98.0, 103.0, 500, 0);
    EXPECT_DOUBLE_EQ(bar.open, 100.0);   // Open unchanged
    EXPECT_DOUBLE_EQ(bar.high, 105.0);   // New high
    EXPECT_DOUBLE_EQ(bar.low, 98.0);     // New low
    EXPECT_DOUBLE_EQ(bar.close, 103.0);  // New close
    EXPECT_DOUBLE_EQ(bar.volume, 1500);  // Accumulated
}

TEST_F(ResamplerTest, ResamplerConfig) {
    ResamplerConfig config(TimeFrame::Minutes, 5);
    EXPECT_EQ(config.timeframe, TimeFrame::Minutes);
    EXPECT_EQ(config.compression, 5);
    EXPECT_TRUE(config.bar2edge);
    EXPECT_TRUE(config.adjbartime);
    EXPECT_TRUE(config.rightedge);
}

TEST_F(ResamplerTest, ResamplerTickCompression) {
    ResamplerConfig config;
    config.timeframe = TimeFrame::Ticks;
    config.compression = 3;
    config.bar2edge = false;  // Simple count-based
    
    Resampler resampler(config);
    
    // Process 3 ticks - should complete one bar
    EXPECT_FALSE(resampler.process(1.0, 100, 101, 99, 100, 100, 0));
    EXPECT_FALSE(resampler.process(1.1, 101, 102, 100, 101, 100, 0));
    EXPECT_TRUE(resampler.process(1.2, 102, 103, 101, 102, 100, 0));  // 3rd tick completes bar
    
    auto bars = resampler.takeCompletedBars();
    EXPECT_EQ(bars.size(), 1u);
    EXPECT_DOUBLE_EQ(bars[0].open, 100.0);
    EXPECT_DOUBLE_EQ(bars[0].high, 103.0);  // Highest of all ticks
    EXPECT_DOUBLE_EQ(bars[0].low, 99.0);    // Lowest of all ticks
    EXPECT_DOUBLE_EQ(bars[0].close, 102.0); // Last tick's close
    EXPECT_DOUBLE_EQ(bars[0].volume, 300);  // Sum of all volumes
}

TEST_F(ResamplerTest, ResamplerFlush) {
    ResamplerConfig config;
    config.timeframe = TimeFrame::Ticks;
    config.compression = 5;
    config.bar2edge = false;
    
    Resampler resampler(config);
    
    // Process 2 ticks (not enough to complete)
    resampler.process(1.0, 100, 101, 99, 100, 100, 0);
    resampler.process(1.1, 101, 102, 100, 101, 100, 0);
    
    EXPECT_TRUE(resampler.hasPendingBar());
    EXPECT_EQ(resampler.completedBars().size(), 0u);
    
    // Flush incomplete bar
    EXPECT_TRUE(resampler.flush());
    
    auto bars = resampler.takeCompletedBars();
    EXPECT_EQ(bars.size(), 1u);
}

TEST_F(ResamplerTest, ResamplerReset) {
    ResamplerConfig config(TimeFrame::Ticks, 2);
    config.bar2edge = false;
    
    Resampler resampler(config);
    
    resampler.process(1.0, 100, 101, 99, 100, 100, 0);
    resampler.process(1.1, 101, 102, 100, 101, 100, 0);
    
    EXPECT_EQ(resampler.completedBars().size(), 1u);
    
    resampler.reset();
    
    EXPECT_FALSE(resampler.hasPendingBar());
    EXPECT_EQ(resampler.completedBars().size(), 0u);
}

// ==================== Signal Strategy Tests ====================

class SignalStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(SignalStrategyTest, SignalAccumMode) {
    SignalStrategy strategy;
    
    EXPECT_EQ(strategy.signalAccumMode(), SignalAccumMode::LongShort);
    
    strategy.setSignalAccumMode(SignalAccumMode::LongOnly);
    EXPECT_EQ(strategy.signalAccumMode(), SignalAccumMode::LongOnly);
    
    strategy.setSignalAccumMode(SignalAccumMode::ShortOnly);
    EXPECT_EQ(strategy.signalAccumMode(), SignalAccumMode::ShortOnly);
}

TEST_F(SignalStrategyTest, ExitOnOpposite) {
    SignalStrategy strategy;
    
    EXPECT_TRUE(strategy.exitOnOpposite());
    
    strategy.setExitOnOpposite(false);
    EXPECT_FALSE(strategy.exitOnOpposite());
}

// Note: main() is provided by gtest_main library

