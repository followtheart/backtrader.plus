/**
 * @file test_lineseries.cpp
 * @brief LineSeries 单元测试
 */

#include <gtest/gtest.h>
#include "bt/lineseries.hpp"

using namespace bt;

class LineSeriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试 OHLCV 数据
        ohlcvData = {
            {100.0, 105.0, 98.0, 102.0, 1000.0},
            {102.0, 108.0, 101.0, 107.0, 1500.0},
            {107.0, 110.0, 105.0, 109.0, 1200.0},
            {109.0, 112.0, 107.0, 110.0, 1800.0},
            {110.0, 115.0, 109.0, 114.0, 2000.0},
        };
    }
    
    struct Bar {
        Value open, high, low, close, volume;
    };
    std::vector<Bar> ohlcvData;
};

TEST_F(LineSeriesTest, AddLine) {
    LineSeries series;
    Size idx = series.addLine("test");
    
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(series.numLines(), 1);
    EXPECT_TRUE(series.hasLine("test"));
}

TEST_F(LineSeriesTest, MultipleLine) {
    LineSeries series;
    series.addLine("line1");
    series.addLine("line2");
    series.addLine("line3");
    
    EXPECT_EQ(series.numLines(), 3);
}

TEST_F(LineSeriesTest, AccessByIndex) {
    LineSeries series;
    series.addLine("test");
    
    series.line(0).push(42.0);
    EXPECT_DOUBLE_EQ(series.line(0)[0], 42.0);
}

TEST_F(LineSeriesTest, AccessByName) {
    LineSeries series;
    series.addLine("price");
    
    series.line("price").push(100.0);
    EXPECT_DOUBLE_EQ(series.line("price")[0], 100.0);
}

TEST_F(LineSeriesTest, DefaultLineAccess) {
    LineSeries series;
    series.addLine("main");
    series.line(0).push(50.0);
    
    // [] 操作符访问第一条线
    EXPECT_DOUBLE_EQ(series[0], 50.0);
}

TEST_F(LineSeriesTest, OHLCVDataStructure) {
    OHLCVData data;
    
    EXPECT_EQ(data.numLines(), 6);  // open, high, low, close, volume, openinterest
    EXPECT_TRUE(data.hasLine("open"));
    EXPECT_TRUE(data.hasLine("high"));
    EXPECT_TRUE(data.hasLine("low"));
    EXPECT_TRUE(data.hasLine("close"));
    EXPECT_TRUE(data.hasLine("volume"));
    EXPECT_TRUE(data.hasLine("openinterest"));
}

TEST_F(LineSeriesTest, AddBar) {
    OHLCVData data;
    
    for (const auto& bar : ohlcvData) {
        data.addBar(bar.open, bar.high, bar.low, bar.close, bar.volume);
    }
    
    EXPECT_EQ(data.open().size(), 5);
    // 需要先定位到数据末尾
    for (Size i = 0; i < ohlcvData.size() - 1; ++i) {
        data.advance();
    }
    EXPECT_DOUBLE_EQ(data.open()[0], 110.0);    // 最后一个 bar
    EXPECT_DOUBLE_EQ(data.high()[0], 115.0);
    EXPECT_DOUBLE_EQ(data.close()[0], 114.0);
}

TEST_F(LineSeriesTest, Advance) {
    OHLCVData data;
    for (const auto& bar : ohlcvData) {
        data.addBar(bar.open, bar.high, bar.low, bar.close, bar.volume);
    }
    
    data.home();
    EXPECT_DOUBLE_EQ(data.close()[0], 102.0);  // 第一个 bar
    
    data.advance();
    EXPECT_DOUBLE_EQ(data.close()[0], 107.0);  // 第二个 bar
}

TEST_F(LineSeriesTest, MinPeriod) {
    LineSeries series;
    series.addLine("line1");
    series.addLine("line2");
    
    series.line(0).setMinperiod(5);
    series.line(1).setMinperiod(10);
    
    // 整个 series 的 minperiod 是所有线的最大值
    EXPECT_EQ(series.minperiod(), 10);
}

TEST_F(LineSeriesTest, ReadyState) {
    OHLCVData data;
    data.setMinperiod(3);
    
    data.addBar(100, 105, 98, 102, 1000);
    EXPECT_FALSE(data.ready());
    
    data.addBar(102, 108, 101, 107, 1500);
    EXPECT_FALSE(data.ready());
    
    data.addBar(107, 110, 105, 109, 1200);
    EXPECT_TRUE(data.ready());
}

TEST_F(LineSeriesTest, LineInfo) {
    LineSeries series;
    series.addLine("alpha");
    series.addLine("beta");
    
    const auto& infos = series.lineInfos();
    EXPECT_EQ(infos.size(), 2);
    EXPECT_EQ(infos[0].name, "alpha");
    EXPECT_EQ(infos[0].index, 0);
    EXPECT_EQ(infos[1].name, "beta");
    EXPECT_EQ(infos[1].index, 1);
}
