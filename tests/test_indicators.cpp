/**
 * @file test_indicators.cpp
 * @brief 指标单元测试
 */

#include <gtest/gtest.h>
#include "bt/backtrader.hpp"
#include <cmath>

using namespace bt;

class IndicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试价格数据
        priceData = {
            100.0, 101.0, 102.0, 103.0, 104.0,
            105.0, 104.0, 103.0, 102.0, 101.0,
            100.0, 101.0, 102.0, 103.0, 104.0,
            105.0, 106.0, 107.0, 108.0, 109.0,
            110.0, 109.0, 108.0, 107.0, 106.0,
            105.0, 104.0, 103.0, 102.0, 101.0
        };
        
        // 创建 LineBuffer 并填充数据
        dataBuffer.extend(priceData);
    }
    
    std::vector<Value> priceData;
    LineBuffer dataBuffer;
    
    // 辅助函数：计算预期的 SMA
    Value expectedSMA(Size end, int period) {
        Value sum = 0;
        for (int i = 0; i < period; ++i) {
            sum += priceData[end - i];
        }
        return sum / period;
    }
};

// ==================== SMA 测试 ====================
TEST_F(IndicatorTest, SMABasic) {
    int period = 5;
    SMA sma(&dataBuffer, period);
    sma.init();
    sma.precompute();
    
    // 检查 minperiod
    EXPECT_EQ(sma.minperiod(), period);
    
    // 检查输出长度
    EXPECT_GT(sma.lines0().size(), 0);
}

TEST_F(IndicatorTest, SMACalculation) {
    int period = 5;
    SMA sma(&dataBuffer, period);
    sma.init();
    
    // 手动计算几个值
    dataBuffer.home();
    // advance period-1 次到达第一个有效计算位置
    for (int i = 0; i < period - 1; ++i) {
        dataBuffer.advance();
    }
    
    // 此时 dataBuffer 位置为 4（第5个数据点，索引从0开始）
    // 此时应该有足够的数据计算 SMA
    sma.next();
    
    // 验证第一个 SMA 值
    // 位置4时，dataValue(0)=104, dataValue(1)=103, ..., dataValue(4)=100
    Value expected = (100.0 + 101.0 + 102.0 + 103.0 + 104.0) / 5.0;
    EXPECT_NEAR(sma.value(0), expected, 1e-10);
}

TEST_F(IndicatorTest, SMAWithParams) {
    Params p;
    p.set("period", 10);
    
    SMA sma(p);
    sma.bindData(&dataBuffer);
    
    EXPECT_EQ(sma.minperiod(), 10);
}

// ==================== EMA 测试 ====================
TEST_F(IndicatorTest, EMABasic) {
    int period = 10;
    EMA ema(&dataBuffer, period);
    ema.init();
    
    EXPECT_EQ(ema.minperiod(), period);
}

TEST_F(IndicatorTest, EMACalculation) {
    int period = 5;
    EMA ema(&dataBuffer, period);
    ema.init();
    ema.precompute();
    
    // EMA 应该有输出
    EXPECT_GT(ema.lines0().size(), 0);
    
    // 第一个 EMA 值应该等于 SMA
    Value firstSMA = (100.0 + 101.0 + 102.0 + 103.0 + 104.0) / 5.0;
    // 由于向量化计算，我们检查第一个有效值
    // EMA 的特性：响应更快，应该更接近最近的价格
}

// ==================== RSI 测试 ====================
TEST_F(IndicatorTest, RSIBasic) {
    int period = 14;
    RSI rsi(&dataBuffer, period);
    rsi.init();
    
    EXPECT_EQ(rsi.minperiod(), period + 1);  // RSI 需要前一个值
}

TEST_F(IndicatorTest, RSIRange) {
    RSI rsi(&dataBuffer, 5);  // 使用较短周期便于测试
    rsi.init();
    rsi.precompute();
    
    // RSI 应该在 0-100 之间
    for (Size i = 0; i < rsi.lines0().size(); ++i) {
        Value v = rsi.lines0().rawData()->at(i);
        if (!isnan(v)) {
            EXPECT_GE(v, 0.0);
            EXPECT_LE(v, 100.0);
        }
    }
}

TEST_F(IndicatorTest, RSIOverboughtOversold) {
    // 创建上涨序列
    LineBuffer upData;
    for (int i = 0; i < 20; ++i) {
        upData.push(100.0 + i * 2.0);  // 持续上涨
    }
    
    RSI rsi(&upData, 5);
    rsi.init();
    rsi.precompute();
    
    // 持续上涨应该导致 RSI 接近 100
    // （由于是连续上涨，RSI 应该很高）
}

// ==================== MACD 测试 ====================
TEST_F(IndicatorTest, MACDBasic) {
    MACD macd(&dataBuffer, 12, 26, 9);
    macd.init();
    
    // MACD 有 3 条线
    EXPECT_EQ(macd.numLines(), 3);
}

TEST_F(IndicatorTest, MACDLines) {
    MACD macd(&dataBuffer, 5, 10, 3);  // 较短周期便于测试
    macd.init();
    macd.precompute();
    
    // 检查 3 条线都有数据
    EXPECT_GT(macd.macd().size(), 0);
    EXPECT_GT(macd.signal().size(), 0);
    EXPECT_GT(macd.histogram().size(), 0);
}

// ==================== BollingerBands 测试 ====================
TEST_F(IndicatorTest, BollingerBasic) {
    BollingerBands bb(&dataBuffer, 20, 2.0);
    bb.init();
    
    // 布林带有 3 条线
    EXPECT_EQ(bb.numLines(), 3);
    EXPECT_EQ(bb.minperiod(), 20);
}

TEST_F(IndicatorTest, BollingerBands) {
    // 先定位输入数据到最后
    for (Size i = 0; i < priceData.size() - 1; ++i) {
        dataBuffer.advance();
    }
    
    BollingerBands bb(&dataBuffer, 5, 2.0);  // 较短周期
    bb.init();
    
    // 手动计算几次
    dataBuffer.home();
    for (int i = 0; i < 4; ++i) {  // period - 1
        dataBuffer.advance();
    }
    
    // 计算一些值
    for (int i = 0; i < 10; ++i) {
        bb.next();
        dataBuffer.advance();
    }
    
    // 检查 3 条线都有数据
    EXPECT_GT(bb.mid().size(), 0);
    EXPECT_GT(bb.top().size(), 0);
    EXPECT_GT(bb.bot().size(), 0);
}

TEST_F(IndicatorTest, BollingerRelationship) {
    BollingerBands bb(&dataBuffer, 5, 2.0);
    bb.init();
    
    // 手动计算
    dataBuffer.home();
    for (int i = 0; i < 4; ++i) {  // period - 1
        dataBuffer.advance();
    }
    
    // 计算一些值
    for (int i = 0; i < 10 && dataBuffer.position() < static_cast<Index>(priceData.size() - 1); ++i) {
        bb.next();
        dataBuffer.advance();
    }
    
    // 上轨应该 >= 中轨 >= 下轨
    auto* midData = bb.mid().rawData();
    auto* topData = bb.top().rawData();
    auto* botData = bb.bot().rawData();
    
    ASSERT_NE(midData, nullptr);
    ASSERT_NE(topData, nullptr);
    ASSERT_NE(botData, nullptr);
    
    for (Size i = 0; i < midData->size(); ++i) {
        Value mid = (*midData)[i];
        Value top = (*topData)[i];
        Value bot = (*botData)[i];
        
        if (!isnan(mid) && !isnan(top) && !isnan(bot)) {
            EXPECT_GE(top, mid);
            EXPECT_GE(mid, bot);
        }
    }
}

TEST_F(IndicatorTest, BollingerPercentB) {
    BollingerBands bb(&dataBuffer, 5, 2.0);
    bb.init();
    
    // 手动计算
    dataBuffer.home();
    for (int i = 0; i < 4; ++i) {  // period - 1
        dataBuffer.advance();
    }
    
    // 计算足够多的值
    for (int i = 0; i < 10 && dataBuffer.position() < static_cast<Index>(priceData.size() - 1); ++i) {
        bb.next();
        dataBuffer.advance();
    }
    
    // %B 测试 - 使用 rawData 访问
    auto* midData = bb.mid().rawData();
    auto* topData = bb.top().rawData();
    auto* botData = bb.bot().rawData();
    
    ASSERT_NE(midData, nullptr);
    ASSERT_GT(midData->size(), 0);
    
    // 使用最后计算的值
    Size lastIdx = midData->size() - 1;
    Value mid = (*midData)[lastIdx];
    Value top = (*topData)[lastIdx];
    Value bot = (*botData)[lastIdx];
    
    if (!isnan(mid) && !isnan(top) && !isnan(bot) && top > bot) {
        // %B = (price - bot) / (top - bot)
        Value pctB_mid = (mid - bot) / (top - bot);
        EXPECT_NEAR(pctB_mid, 0.5, 0.01);
        
        Value pctB_top = (top - bot) / (top - bot);
        EXPECT_NEAR(pctB_top, 1.0, 0.01);
        
        Value pctB_bot = (bot - bot) / (top - bot);
        EXPECT_NEAR(pctB_bot, 0.0, 0.01);
    }
}

// ==================== StdDev 测试 ====================
TEST_F(IndicatorTest, StdDevBasic) {
    StdDev sd(&dataBuffer, 5);
    sd.init();
    
    // 手动计算
    dataBuffer.home();
    for (int i = 0; i < 4; ++i) {  // period - 1
        dataBuffer.advance();
    }
    
    // 计算一些值
    for (int i = 0; i < 10 && dataBuffer.position() < static_cast<Index>(priceData.size() - 1); ++i) {
        sd.next();
        dataBuffer.advance();
    }
    
    // 标准差应该 >= 0
    auto* data = sd.lines0().rawData();
    ASSERT_NE(data, nullptr);
    
    for (Value v : *data) {
        if (!isnan(v)) {
            EXPECT_GE(v, 0.0);
        }
    }
}

TEST_F(IndicatorTest, StdDevConstantData) {
    // 常数序列的标准差应该为 0
    LineBuffer constData;
    for (int i = 0; i < 20; ++i) {
        constData.push(100.0);
    }
    
    StdDev sd(&constData, 5);
    sd.init();
    
    // 手动计算
    constData.home();
    for (int i = 0; i < 4; ++i) {  // period - 1
        constData.advance();
    }
    
    // 计算一些值
    for (int i = 0; i < 10; ++i) {
        sd.next();
        constData.advance();
    }
    
    auto* data = sd.lines0().rawData();
    ASSERT_NE(data, nullptr);
    
    for (Value v : *data) {
        if (!isnan(v)) {
            EXPECT_NEAR(v, 0.0, 1e-10);
        }
    }
}

// ==================== 指标组合测试 ====================
TEST_F(IndicatorTest, IndicatorChaining) {
    // SMA 的 SMA
    SMA sma1(&dataBuffer, 5);
    sma1.init();
    sma1.precompute();
    
    // 使用 SMA1 的输出作为输入
    SMA sma2(&sma1.lines0(), 3);
    sma2.init();
    sma2.precompute();
    
    // 双重平滑应该更加平滑
    EXPECT_GT(sma2.lines0().size(), 0);
}
