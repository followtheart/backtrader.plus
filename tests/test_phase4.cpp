/**
 * @file test_phase4.cpp
 * @brief Phase 4 测试 - 性能优化
 * 
 * 测试内容:
 * 1. SIMD 数学运算
 * 2. 向量化指标计算
 * 3. 线程池
 * 4. 参数优化
 */

#include <gtest/gtest.h>
#include "bt/backtrader.hpp"
#include <chrono>
#include <numeric>
#include <random>
#include <cmath>

using namespace bt;

// ============================================================================
// SIMD 数学运算测试
// ============================================================================

class SIMDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 生成测试数据
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(1.0, 100.0);
        
        testData.resize(10000);
        for (auto& v : testData) {
            v = dist(gen);
        }
        
        testDataB.resize(10000);
        for (auto& v : testDataB) {
            v = dist(gen);
        }
    }
    
    std::vector<double> testData;
    std::vector<double> testDataB;
};

TEST_F(SIMDTest, GetSIMDLevel) {
    const char* level = simd::getSIMDLevel();
    ASSERT_NE(level, nullptr);
    // 至少应该有 Scalar 支持
    EXPECT_TRUE(
        std::string(level) == "AVX-512" ||
        std::string(level) == "AVX2" ||
        std::string(level) == "AVX" ||
        std::string(level) == "SSE2" ||
        std::string(level) == "Scalar"
    );
}

TEST_F(SIMDTest, VectorSum) {
    double simdSum = simd::sum(testData.data(), testData.size());
    
    // 验证结果
    double expectedSum = std::accumulate(testData.begin(), testData.end(), 0.0);
    EXPECT_NEAR(simdSum, expectedSum, 1e-6);
}

TEST_F(SIMDTest, VectorMean) {
    double simdMean = simd::mean(testData.data(), testData.size());
    
    double expectedMean = std::accumulate(testData.begin(), testData.end(), 0.0) / testData.size();
    EXPECT_NEAR(simdMean, expectedMean, 1e-6);
}

TEST_F(SIMDTest, VectorAdd) {
    std::vector<double> result(testData.size());
    simd::add(testData.data(), testDataB.data(), result.data(), testData.size());
    
    for (Size i = 0; i < testData.size(); ++i) {
        EXPECT_NEAR(result[i], testData[i] + testDataB[i], 1e-10);
    }
}

TEST_F(SIMDTest, VectorSub) {
    std::vector<double> result(testData.size());
    simd::sub(testData.data(), testDataB.data(), result.data(), testData.size());
    
    for (Size i = 0; i < testData.size(); ++i) {
        EXPECT_NEAR(result[i], testData[i] - testDataB[i], 1e-10);
    }
}

TEST_F(SIMDTest, VectorMul) {
    std::vector<double> result(testData.size());
    simd::mul(testData.data(), testDataB.data(), result.data(), testData.size());
    
    for (Size i = 0; i < testData.size(); ++i) {
        EXPECT_NEAR(result[i], testData[i] * testDataB[i], 1e-6);
    }
}

TEST_F(SIMDTest, VectorDot) {
    double simdDot = simd::dot(testData.data(), testDataB.data(), testData.size());
    
    double expectedDot = 0;
    for (Size i = 0; i < testData.size(); ++i) {
        expectedDot += testData[i] * testDataB[i];
    }
    EXPECT_NEAR(simdDot, expectedDot, 1e-3);
}

TEST_F(SIMDTest, SlidingMean) {
    Size window = 20;
    std::vector<double> result(testData.size());
    simd::slidingMean(testData.data(), result.data(), testData.size(), window);
    
    // 验证一些点
    for (Size i = window - 1; i < testData.size(); i += 100) {
        double expectedMean = 0;
        for (Size j = i - window + 1; j <= i; ++j) {
            expectedMean += testData[j];
        }
        expectedMean /= window;
        EXPECT_NEAR(result[i], expectedMean, 1e-10);
    }
}

TEST_F(SIMDTest, EMA) {
    Size period = 14;
    std::vector<double> result(testData.size());
    simd::ema(testData.data(), result.data(), testData.size(), period);
    
    // 前 period-1 个应该是 NaN
    for (Size i = 0; i < period - 1; ++i) {
        EXPECT_TRUE(std::isnan(result[i]));
    }
    
    // 验证第一个有效值（SMA）
    double firstSMA = 0;
    for (Size i = 0; i < period; ++i) {
        firstSMA += testData[i];
    }
    firstSMA /= period;
    EXPECT_NEAR(result[period - 1], firstSMA, 1e-10);
}

// ============================================================================
// 向量化指标测试
// ============================================================================

class VectorizedIndicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 生成模拟价格数据
        prices.reserve(1000);
        double price = 100.0;
        std::mt19937 gen(42);
        std::normal_distribution<double> dist(0, 1);
        
        for (int i = 0; i < 1000; ++i) {
            price += dist(gen) * 0.5;
            if (price < 10) price = 10;
            prices.push(price);
        }
    }
    
    LineBuffer prices;
};

TEST_F(VectorizedIndicatorTest, SMAOnce) {
    SMA sma(&prices, 20);
    sma.init();
    
    // 预计算（使用 once）
    sma.precompute();
    
    // 验证结果
    EXPECT_GT(sma.lines0().length(), 0u);
    
    // 检查值是否合理
    Value lastSMA = sma.value(0);
    EXPECT_FALSE(std::isnan(lastSMA));
    EXPECT_GT(lastSMA, 0);
}

TEST_F(VectorizedIndicatorTest, EMAOnce) {
    EMA ema(&prices, 20);
    ema.init();
    ema.precompute();
    
    EXPECT_GT(ema.lines0().length(), 0u);
    
    Value lastEMA = ema.value(0);
    EXPECT_FALSE(std::isnan(lastEMA));
    EXPECT_GT(lastEMA, 0);
}

TEST_F(VectorizedIndicatorTest, RSIOnce) {
    RSI rsi(&prices, 14);
    rsi.init();
    rsi.precompute();
    
    // RSI 应该在 0-100 范围内
    Value lastRSI = rsi.value(0);
    if (!std::isnan(lastRSI)) {
        EXPECT_GE(lastRSI, 0);
        EXPECT_LE(lastRSI, 100);
    }
}

TEST_F(VectorizedIndicatorTest, MACDOnce) {
    MACD macd(&prices, 12, 26, 9);
    macd.init();
    macd.precompute();
    
    // MACD 线应该有值
    EXPECT_GT(macd.macd().length(), 0u);
}

// ============================================================================
// 线程池测试
// ============================================================================

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ThreadPool>(4);
    }
    
    std::unique_ptr<ThreadPool> pool;
};

TEST_F(ThreadPoolTest, BasicSubmit) {
    auto future = pool->submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, MultipleSubmits) {
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool->submit([i]() { return i * 2; }));
    }
    
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST_F(ThreadPoolTest, Map) {
    std::vector<int> inputs = {1, 2, 3, 4, 5};
    auto results = pool->map([](int x) { return x * x; }, inputs);
    
    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 4);
    EXPECT_EQ(results[2], 9);
    EXPECT_EQ(results[3], 16);
    EXPECT_EQ(results[4], 25);
}

TEST_F(ThreadPoolTest, WaitAll) {
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; ++i) {
        pool->submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++counter;
        });
    }
    
    pool->waitAll();
    EXPECT_EQ(counter.load(), 100);
}

TEST_F(ThreadPoolTest, ParallelFor) {
    std::vector<int> data(1000, 0);
    
    parallelFor(*pool, 0u, data.size(), [&data](Size i) {
        data[i] = static_cast<int>(i * 2);
    });
    
    for (Size i = 0; i < data.size(); ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i * 2));
    }
}

// ============================================================================
// 参数网格测试
// ============================================================================

TEST(ParameterGridTest, SingleParam) {
    ParameterGrid grid;
    grid.addParamInt("period", 10, 15);
    
    auto combinations = grid.generate();
    EXPECT_EQ(combinations.size(), 6u);  // 10, 11, 12, 13, 14, 15
}

TEST(ParameterGridTest, MultipleParams) {
    ParameterGrid grid;
    grid.addParamInt("fast", 10, 12);    // 3 values
    grid.addParamInt("slow", 20, 22);    // 3 values
    
    auto combinations = grid.generate();
    EXPECT_EQ(combinations.size(), 9u);  // 3 * 3
}

TEST(ParameterGridTest, TotalCombinations) {
    ParameterGrid grid;
    grid.addParamInt("a", 1, 5);   // 5 values
    grid.addParamInt("b", 1, 3);   // 3 values
    grid.addParamInt("c", 1, 2);   // 2 values
    
    EXPECT_EQ(grid.totalCombinations(), 30u);  // 5 * 3 * 2
}

// ============================================================================
// VectorMath 测试
// ============================================================================

TEST(VectorMathTest, Sum) {
    std::vector<Value> data = {1, 2, 3, 4, 5};
    Value sum = VectorMath::sum(data.data(), data.size());
    EXPECT_NEAR(sum, 15.0, 1e-10);
}

TEST(VectorMathTest, Mean) {
    std::vector<Value> data = {1, 2, 3, 4, 5};
    Value mean = VectorMath::mean(data.data(), data.size());
    EXPECT_NEAR(mean, 3.0, 1e-10);
}

TEST(VectorMathTest, StdDev) {
    std::vector<Value> data = {2, 4, 4, 4, 5, 5, 7, 9};
    Value mean = VectorMath::mean(data.data(), data.size());
    Value sd = VectorMath::stddev(data.data(), data.size(), mean);
    EXPECT_NEAR(sd, 2.0, 0.01);
}

TEST(VectorMathTest, MaxMin) {
    std::vector<Value> data = {3, 1, 4, 1, 5, 9, 2, 6};
    EXPECT_NEAR(VectorMath::max(data.data(), data.size()), 9.0, 1e-10);
    EXPECT_NEAR(VectorMath::min(data.data(), data.size()), 1.0, 1e-10);
}

TEST(VectorMathTest, SlidingSum) {
    std::vector<Value> data = {1, 2, 3, 4, 5};
    std::vector<Value> result(5);
    
    VectorMath::slidingSum(data.data(), result.data(), 5, 3);
    
    EXPECT_TRUE(std::isnan(result[0]));  // Not enough data
    EXPECT_TRUE(std::isnan(result[1]));
    EXPECT_NEAR(result[2], 6.0, 1e-10);  // 1+2+3
    EXPECT_NEAR(result[3], 9.0, 1e-10);  // 2+3+4
    EXPECT_NEAR(result[4], 12.0, 1e-10); // 3+4+5
}

TEST(VectorMathTest, SlidingMean) {
    std::vector<Value> data = {1, 2, 3, 4, 5};
    std::vector<Value> result(5);
    
    VectorMath::slidingMean(data.data(), result.data(), 5, 3);
    
    EXPECT_NEAR(result[2], 2.0, 1e-10);  // (1+2+3)/3
    EXPECT_NEAR(result[3], 3.0, 1e-10);  // (2+3+4)/3
    EXPECT_NEAR(result[4], 4.0, 1e-10);  // (3+4+5)/3
}

TEST(VectorMathTest, EMA) {
    std::vector<Value> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<Value> result(10);
    Size period = 5;
    Value alpha = 2.0 / (period + 1);
    
    VectorMath::ema(data.data(), result.data(), 10, alpha, period - 1);
    
    // 前 period-1 个应该是 NaN
    for (Size i = 0; i < period - 1; ++i) {
        EXPECT_TRUE(std::isnan(result[i]));
    }
    
    // 第一个有效值是 SMA
    EXPECT_NEAR(result[period - 1], 3.0, 1e-10);  // (1+2+3+4+5)/5
}

// ============================================================================
// 性能基准测试
// ============================================================================

class PerformanceBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // 生成大量数据
        data.resize(100000);
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(1.0, 100.0);
        for (auto& v : data) {
            v = dist(gen);
        }
    }
    
    std::vector<double> data;
};

TEST_F(PerformanceBenchmark, SIMDSumPerformance) {
    const int iterations = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    double result = 0;
    for (int i = 0; i < iterations; ++i) {
        result = simd::sum(data.data(), data.size());
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 只要能运行就通过，主要是记录性能
    EXPECT_GT(result, 0);
    
    // 输出性能信息（可选）
    // std::cout << "SIMD sum: " << duration.count() / iterations << " us per iteration\n";
}

TEST_F(PerformanceBenchmark, SlidingMeanPerformance) {
    const int iterations = 10;
    std::vector<double> result(data.size());
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        simd::slidingMean(data.data(), result.data(), data.size(), 20);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(std::isnan(result.back()));
}

// ============================================================================
// 优化结果分析测试
// ============================================================================

TEST(OptResultAnalyzerTest, Summary) {
    std::vector<OptResult> results;
    
    for (int i = 0; i < 10; ++i) {
        OptResult r;
        r.pnlPct = static_cast<Value>(i * 10 - 20);  // -20, -10, 0, 10, 20, 30, 40, 50, 60, 70
        r.winRate = 50 + i;
        r.totalTrades = 10 + i;
        results.push_back(r);
    }
    
    OptResultAnalyzer analyzer(results);
    auto summary = analyzer.summary();
    
    EXPECT_EQ(summary.totalRuns, 10u);
    EXPECT_EQ(summary.profitableRuns, 7u);  // pnlPct > 0: 10, 20, 30, 40, 50, 60, 70
    EXPECT_NEAR(summary.avgPnlPct, 25.0, 0.01);  // 平均值 = (-20-10+0+10+20+30+40+50+60+70)/10 = 250/10 = 25
    EXPECT_NEAR(summary.maxPnlPct, 70.0, 0.01);  // 最大值是 70
    EXPECT_NEAR(summary.minPnlPct, -20.0, 0.01);
}

TEST(OptResultAnalyzerTest, ParameterSensitivity) {
    std::vector<OptResult> results;
    
    // 添加一些结果，period=10 的表现较好
    for (int period = 10; period <= 20; period += 5) {
        for (int run = 0; run < 3; ++run) {
            OptResult r;
            r.params["period"] = period;
            r.pnlPct = (period == 10) ? 30.0 : 10.0;
            results.push_back(r);
        }
    }
    
    OptResultAnalyzer analyzer(results);
    auto sensitivity = analyzer.parameterSensitivity("period");
    
    EXPECT_EQ(sensitivity.size(), 3u);  // 10, 15, 20
    
    // period=10 的平均 pnlPct 应该更高
    // 注意：sensitivity 的键是 ParamValue，值是 Value (double)
    Value pnl10 = sensitivity[ParamValue(10)];
    Value pnl15 = sensitivity[ParamValue(15)];
    EXPECT_GT(pnl10, pnl15);
}

// ============================================================================
// 版本信息测试
// ============================================================================

TEST(VersionTest, Phase4Version) {
    EXPECT_STREQ(bt::version(), "0.4.0");
}

TEST(VersionTest, SIMDInfo) {
    const char* info = bt::simdInfo();
    ASSERT_NE(info, nullptr);
    EXPECT_GT(strlen(info), 0u);
}

// 注：main 函数已在 test_cerebro.cpp 中定义
