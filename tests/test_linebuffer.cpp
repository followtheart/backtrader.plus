/**
 * @file test_linebuffer.cpp
 * @brief LineBuffer 单元测试
 */

#include <gtest/gtest.h>
#include "bt/linebuffer.hpp"

using namespace bt;

class LineBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试数据
        testData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    }
    
    std::vector<Value> testData;
};

// 基础功能测试
TEST_F(LineBufferTest, CreateUnbounded) {
    LineBuffer buf;
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.minperiod(), 1);
}

TEST_F(LineBufferTest, CreateQBuffer) {
    LineBuffer buf(5);
    EXPECT_EQ(buf.size(), 0);
}

TEST_F(LineBufferTest, PushAndAccess) {
    LineBuffer buf;
    for (Value v : testData) {
        buf.push(v);
    }
    EXPECT_EQ(buf.size(), testData.size());
    
    // 移动到最后一个位置后，[0] 是当前值（最后一个）
    // 不调用 advance/home 时，pos_=0，所以 [0] 是第一个值
    // 需要先定位到数据末尾
    for (Size i = 0; i < testData.size() - 1; ++i) {
        buf.advance();
    }
    EXPECT_DOUBLE_EQ(buf[0], 10.0);
    EXPECT_DOUBLE_EQ(buf[1], 9.0);
}

TEST_F(LineBufferTest, IndexingAfterAdvance) {
    LineBuffer buf;
    buf.extend(testData);
    
    buf.home();  // 回到起点
    EXPECT_DOUBLE_EQ(buf[0], 1.0);
    
    buf.advance();
    EXPECT_DOUBLE_EQ(buf[0], 2.0);
    EXPECT_DOUBLE_EQ(buf[1], 1.0);  // 过去的值
}

TEST_F(LineBufferTest, NegativeIndexing) {
    LineBuffer buf;
    buf.extend(testData);
    
    buf.home();
    buf.advance();  // 现在在位置 1
    
    // [-1] 是未来的值
    EXPECT_DOUBLE_EQ(buf[-1], 3.0);
}

TEST_F(LineBufferTest, QBufferLimitsSize) {
    LineBuffer buf(5);
    buf.extend(testData);
    
    // QBuffer 只保留最后 5 个值
    EXPECT_EQ(buf.size(), 5);
    EXPECT_DOUBLE_EQ(buf[0], 10.0);  // 最新
    EXPECT_DOUBLE_EQ(buf[4], 6.0);   // 最旧（5个值中的第一个）
}

TEST_F(LineBufferTest, MinPeriod) {
    LineBuffer buf;
    buf.setMinperiod(5);
    EXPECT_EQ(buf.minperiod(), 5);
    
    buf.push(1.0);
    buf.push(2.0);
    buf.push(3.0);
    EXPECT_FALSE(buf.ready());  // 不够 5 个
    
    buf.push(4.0);
    buf.push(5.0);
    EXPECT_TRUE(buf.ready());  // 够了
}

TEST_F(LineBufferTest, UpdateMinPeriod) {
    LineBuffer buf;
    buf.setMinperiod(3);
    buf.updateMinperiod(5);
    EXPECT_EQ(buf.minperiod(), 5);  // 取最大
    
    buf.updateMinperiod(2);
    EXPECT_EQ(buf.minperiod(), 5);  // 不变，因为 2 < 5
}

TEST_F(LineBufferTest, Reset) {
    LineBuffer buf;
    buf.extend(testData);
    EXPECT_GT(buf.size(), 0);
    
    buf.reset();
    EXPECT_EQ(buf.size(), 0);
}

TEST_F(LineBufferTest, CurrentValue) {
    LineBuffer buf;
    buf.push(42.0);
    EXPECT_DOUBLE_EQ(buf.current(), 42.0);
}

// 边界情况测试
TEST_F(LineBufferTest, OutOfRangeAccess) {
    LineBuffer buf;
    buf.push(1.0);
    
    // 非 const 版本会抛出异常
    EXPECT_THROW(buf[100], std::out_of_range);
}

TEST_F(LineBufferTest, EmptyBufferAccess) {
    LineBuffer buf;
    
    // 空缓冲区访问
    EXPECT_THROW(buf[0], std::out_of_range);
}

// 性能相关测试
TEST_F(LineBufferTest, RawDataAccess) {
    LineBuffer buf;
    buf.extend(testData);
    
    auto* raw = buf.rawData();
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->size(), testData.size());
}
