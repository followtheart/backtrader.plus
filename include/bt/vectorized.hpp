/**
 * @file vectorized.hpp
 * @brief 向量化计算框架 - 对应 Python 的 runonce 模式
 * 
 * 实现 once/preonce/oncestart 向量化计算模式,
 * 提供批量处理接口以提高性能。
 * 
 * Python lineiterator.py 中的对应方法:
 * - _once(): 主入口，设置缓冲区大小并调用子方法
 * - preonce(start, end): 预热阶段 [0, minperiod-1)
 * - oncestart(start, end): 启动阶段 [minperiod-1, minperiod)
 * - once(start, end): 主计算阶段 [minperiod, buflen)
 */

#pragma once

#include "bt/common.hpp"
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

namespace bt {

/**
 * @brief 执行模式枚举
 */
enum class ExecutionMode {
    Next,       // 事件驱动模式（逐bar）
    Once,       // 向量化模式（批量）
    Hybrid      // 混合模式
};

/**
 * @brief 向量化计算接口
 * 
 * 所有支持向量化计算的组件都应该实现此接口
 */
class IVectorized {
public:
    virtual ~IVectorized() = default;
    
    /**
     * @brief 向量化计算主入口
     * 
     * 对应 Python _once() 方法
     * 1. forward(size=buflen) - 扩展缓冲区
     * 2. 对所有子指标调用 _once()
     * 3. home() - 重置索引
     * 4. 调用 preonce/oncestart/once
     */
    virtual void runOnce() = 0;
    
    /**
     * @brief 预热阶段计算
     * @param start 起始索引（包含）
     * @param end 结束索引（不包含）
     * 
     * 在 minperiod 之前的数据点
     * 默认为空实现，某些指标可能需要特殊预热逻辑
     */
    virtual void preonce(Size start, Size end) {
        // 默认空实现
        (void)start;
        (void)end;
    }
    
    /**
     * @brief 启动阶段计算
     * @param start 起始索引（通常是 minperiod-1）
     * @param end 结束索引（通常是 minperiod）
     * 
     * 第一个有效数据点
     * 默认调用 once()
     */
    virtual void oncestart(Size start, Size end) {
        once(start, end);
    }
    
    /**
     * @brief 主计算阶段
     * @param start 起始索引（包含）
     * @param end 结束索引（不包含）
     * 
     * 处理范围 [start, end) 内的所有数据点
     * 子类必须实现此方法
     */
    virtual void once(Size start, Size end) = 0;
    
    /**
     * @brief 检查是否支持向量化
     */
    virtual bool supportsVectorized() const { return true; }
    
    /**
     * @brief 获取最小周期
     */
    virtual Size getMinperiod() const = 0;
    
    /**
     * @brief 获取缓冲区长度
     */
    virtual Size getBufferLength() const = 0;
};

/**
 * @brief 向量化数学运算工具类
 * 
 * 提供常用的向量化运算函数
 * 后续可以用 SIMD 优化替换
 */
class VectorMath {
public:
    /**
     * @brief 向量求和
     */
    static Value sum(const Value* data, Size count) {
        Value result = 0;
        for (Size i = 0; i < count; ++i) {
            result += data[i];
        }
        return result;
    }
    
    /**
     * @brief 向量求平均
     */
    static Value mean(const Value* data, Size count) {
        if (count == 0) return NaN;
        return sum(data, count) / static_cast<Value>(count);
    }
    
    /**
     * @brief 向量求最大值
     */
    static Value max(const Value* data, Size count) {
        if (count == 0) return NaN;
        Value result = data[0];
        for (Size i = 1; i < count; ++i) {
            if (data[i] > result) result = data[i];
        }
        return result;
    }
    
    /**
     * @brief 向量求最小值
     */
    static Value min(const Value* data, Size count) {
        if (count == 0) return NaN;
        Value result = data[0];
        for (Size i = 1; i < count; ++i) {
            if (data[i] < result) result = data[i];
        }
        return result;
    }
    
    /**
     * @brief 向量求标准差
     */
    static Value stddev(const Value* data, Size count, Value mean_val) {
        if (count < 2) return NaN;
        Value sum_sq = 0;
        for (Size i = 0; i < count; ++i) {
            Value diff = data[i] - mean_val;
            sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / static_cast<Value>(count));
    }
    
    /**
     * @brief 向量加法 (a + b -> result)
     */
    static void add(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = a[i] + b[i];
        }
    }
    
    /**
     * @brief 向量减法 (a - b -> result)
     */
    static void sub(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = a[i] - b[i];
        }
    }
    
    /**
     * @brief 向量乘法 (a * b -> result)
     */
    static void mul(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = a[i] * b[i];
        }
    }
    
    /**
     * @brief 向量除法 (a / b -> result)
     */
    static void div(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = (b[i] != 0) ? (a[i] / b[i]) : NaN;
        }
    }
    
    /**
     * @brief 标量加法 (a + scalar -> result)
     */
    static void addScalar(const Value* a, Value scalar, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = a[i] + scalar;
        }
    }
    
    /**
     * @brief 标量乘法 (a * scalar -> result)
     */
    static void mulScalar(const Value* a, Value scalar, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = a[i] * scalar;
        }
    }
    
    /**
     * @brief 向量绝对值
     */
    static void abs(const Value* a, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = std::abs(a[i]);
        }
    }
    
    /**
     * @brief 向量取负
     */
    static void neg(const Value* a, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = -a[i];
        }
    }
    
    /**
     * @brief 向量比较 (a > b ? 1 : 0)
     */
    static void gt(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = (a[i] > b[i]) ? 1.0 : 0.0;
        }
    }
    
    /**
     * @brief 向量比较 (a < b ? 1 : 0)
     */
    static void lt(const Value* a, const Value* b, Value* result, Size count) {
        for (Size i = 0; i < count; ++i) {
            result[i] = (a[i] < b[i]) ? 1.0 : 0.0;
        }
    }
    
    /**
     * @brief 滑动窗口求和
     * @param data 输入数据
     * @param result 输出结果
     * @param dataLen 数据长度
     * @param window 窗口大小
     * 
     * 高效的滑动窗口求和，利用增量计算
     */
    static void slidingSum(const Value* data, Value* result, Size dataLen, Size window) {
        if (dataLen == 0 || window == 0) return;
        
        // 前 window-1 个点设为 NaN
        for (Size i = 0; i < window - 1 && i < dataLen; ++i) {
            result[i] = NaN;
        }
        
        if (dataLen < window) return;
        
        // 计算第一个窗口的和
        Value windowSum = 0;
        for (Size i = 0; i < window; ++i) {
            windowSum += data[i];
        }
        result[window - 1] = windowSum;
        
        // 滑动计算后续的和
        for (Size i = window; i < dataLen; ++i) {
            windowSum = windowSum - data[i - window] + data[i];
            result[i] = windowSum;
        }
    }
    
    /**
     * @brief 滑动窗口求平均
     */
    static void slidingMean(const Value* data, Value* result, Size dataLen, Size window) {
        slidingSum(data, result, dataLen, window);
        
        // 将和转换为平均值
        Value divisor = static_cast<Value>(window);
        for (Size i = window - 1; i < dataLen; ++i) {
            result[i] /= divisor;
        }
    }
    
    /**
     * @brief 滑动窗口求最大值
     */
    static void slidingMax(const Value* data, Value* result, Size dataLen, Size window) {
        if (dataLen == 0 || window == 0) return;
        
        for (Size i = 0; i < window - 1 && i < dataLen; ++i) {
            result[i] = NaN;
        }
        
        for (Size i = window - 1; i < dataLen; ++i) {
            result[i] = max(data + i - window + 1, window);
        }
    }
    
    /**
     * @brief 滑动窗口求最小值
     */
    static void slidingMin(const Value* data, Value* result, Size dataLen, Size window) {
        if (dataLen == 0 || window == 0) return;
        
        for (Size i = 0; i < window - 1 && i < dataLen; ++i) {
            result[i] = NaN;
        }
        
        for (Size i = window - 1; i < dataLen; ++i) {
            result[i] = min(data + i - window + 1, window);
        }
    }
    
    /**
     * @brief EMA 向量化计算
     * @param data 输入数据
     * @param result 输出结果
     * @param dataLen 数据长度
     * @param alpha 平滑因子 (2 / (period + 1))
     * @param initIdx 初始化索引（使用 SMA 初始化的位置）
     */
    static void ema(const Value* data, Value* result, Size dataLen, Value alpha, Size initIdx) {
        if (dataLen == 0) return;
        
        // 前 initIdx 个点设为 NaN
        for (Size i = 0; i < initIdx && i < dataLen; ++i) {
            result[i] = NaN;
        }
        
        if (dataLen <= initIdx) return;
        
        // 用 SMA 初始化
        Value sum = 0;
        Size period = initIdx + 1;
        for (Size i = 0; i <= initIdx; ++i) {
            sum += data[i];
        }
        result[initIdx] = sum / static_cast<Value>(period);
        
        // 递推计算
        Value oneMinusAlpha = 1.0 - alpha;
        for (Size i = initIdx + 1; i < dataLen; ++i) {
            result[i] = alpha * data[i] + oneMinusAlpha * result[i - 1];
        }
    }
};

/**
 * @brief 可向量化数据缓冲区接口
 * 
 * 用于提供原始数据指针访问，以便进行向量化计算
 */
class IVectorBuffer {
public:
    virtual ~IVectorBuffer() = default;
    
    /**
     * @brief 获取原始数据指针
     */
    virtual const Value* rawData() const = 0;
    
    /**
     * @brief 获取可写原始数据指针
     */
    virtual Value* rawDataMutable() = 0;
    
    /**
     * @brief 获取数据长度
     */
    virtual Size dataLength() const = 0;
    
    /**
     * @brief 预分配指定大小的缓冲区
     */
    virtual void reserve(Size capacity) = 0;
    
    /**
     * @brief 调整缓冲区大小
     */
    virtual void resize(Size size) = 0;
};

/**
 * @brief 批量操作结果收集器
 */
template<typename T>
class BatchResult {
public:
    BatchResult() = default;
    
    void reserve(Size count) {
        results_.reserve(count);
    }
    
    void push(T value) {
        results_.push_back(std::move(value));
    }
    
    const std::vector<T>& results() const { return results_; }
    std::vector<T>& results() { return results_; }
    
    Size size() const { return results_.size(); }
    bool empty() const { return results_.empty(); }
    
    T& operator[](Size idx) { return results_[idx]; }
    const T& operator[](Size idx) const { return results_[idx]; }

private:
    std::vector<T> results_;
};

/**
 * @brief 向量化执行统计
 */
struct VectorizedStats {
    Size totalBars = 0;           // 总数据点数
    Size computedBars = 0;        // 计算的数据点数
    Size preonceBars = 0;         // preonce 阶段数据点
    Size oncestartBars = 0;       // oncestart 阶段数据点
    Size onceBars = 0;            // once 阶段数据点
    double computeTimeMs = 0;     // 计算耗时（毫秒）
    
    void reset() {
        totalBars = computedBars = preonceBars = oncestartBars = onceBars = 0;
        computeTimeMs = 0;
    }
};

} // namespace bt
