/**
 * @file simd.hpp
 * @brief SIMD 向量化数学运算库
 * 
 * 使用 SSE/AVX 指令加速数学运算。
 * 在编译时检测可用的 SIMD 指令集并使用最优实现。
 * 
 * 支持的指令集:
 * - SSE2 (128位，2个double)
 * - AVX (256位，4个double)
 * - AVX2 (256位整数运算)
 * - AVX-512 (512位，8个double) - 可选
 * 
 * 回退实现:
 * - 标量实现 (无 SIMD 支持时)
 */

#pragma once

#include "bt/common.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

// SIMD 指令集检测
#if defined(__AVX512F__)
    #define BT_SIMD_AVX512 1
    #define BT_SIMD_LEVEL 512
#elif defined(__AVX2__)
    #define BT_SIMD_AVX2 1
    #define BT_SIMD_LEVEL 256
#elif defined(__AVX__)
    #define BT_SIMD_AVX 1
    #define BT_SIMD_LEVEL 256
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #define BT_SIMD_SSE2 1
    #define BT_SIMD_LEVEL 128
#else
    #define BT_SIMD_NONE 1
    #define BT_SIMD_LEVEL 0
#endif

// 包含 SIMD 头文件
#if BT_SIMD_LEVEL >= 256
    #include <immintrin.h>
#elif BT_SIMD_LEVEL >= 128
    #include <emmintrin.h>
#endif

namespace bt {
namespace simd {

// ============================================================================
// SIMD 信息
// ============================================================================

/**
 * @brief 获取 SIMD 级别描述
 */
inline const char* getSIMDLevel() {
#if defined(BT_SIMD_AVX512)
    return "AVX-512";
#elif defined(BT_SIMD_AVX2)
    return "AVX2";
#elif defined(BT_SIMD_AVX)
    return "AVX";
#elif defined(BT_SIMD_SSE2)
    return "SSE2";
#else
    return "Scalar";
#endif
}

/**
 * @brief 获取 SIMD 向量宽度（double 元素数）
 */
constexpr Size getSIMDWidth() {
#if BT_SIMD_LEVEL >= 512
    return 8;  // AVX-512: 8 doubles
#elif BT_SIMD_LEVEL >= 256
    return 4;  // AVX: 4 doubles
#elif BT_SIMD_LEVEL >= 128
    return 2;  // SSE2: 2 doubles
#else
    return 1;  // Scalar
#endif
}

// ============================================================================
// 基础向量运算 - 条件编译实现
// ============================================================================

#if BT_SIMD_LEVEL >= 256  // AVX/AVX2

/**
 * @brief AVX 向量加法
 */
inline void addAVX(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vr = _mm256_add_pd(va, vb);
        _mm256_storeu_pd(result + i, vr);
    }
    
    // 处理剩余元素
    for (; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief AVX 向量减法
 */
inline void subAVX(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vr = _mm256_sub_pd(va, vb);
        _mm256_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] - b[i];
    }
}

/**
 * @brief AVX 向量乘法
 */
inline void mulAVX(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vr = _mm256_mul_pd(va, vb);
        _mm256_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief AVX 向量除法
 */
inline void divAVX(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vr = _mm256_div_pd(va, vb);
        _mm256_storeu_pd(result + i, vr);
    }
    
    // 处理剩余元素并检查除零
    for (; i < count; ++i) {
        result[i] = (b[i] != 0) ? (a[i] / b[i]) : NaN;
    }
}

/**
 * @brief AVX 标量乘法
 */
inline void mulScalarAVX(const double* a, double scalar, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    __m256d vs = _mm256_set1_pd(scalar);
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vr = _mm256_mul_pd(va, vs);
        _mm256_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] * scalar;
    }
}

/**
 * @brief AVX 向量求和
 */
inline double sumAVX(const double* data, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    __m256d vsum = _mm256_setzero_pd();
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(data + i);
        vsum = _mm256_add_pd(vsum, va);
    }
    
    // 水平求和
    __m128d vlow = _mm256_castpd256_pd128(vsum);
    __m128d vhigh = _mm256_extractf128_pd(vsum, 1);
    vlow = _mm_add_pd(vlow, vhigh);
    __m128d high64 = _mm_unpackhi_pd(vlow, vlow);
    double result = _mm_cvtsd_f64(_mm_add_sd(vlow, high64));
    
    // 处理剩余元素
    for (; i < count; ++i) {
        result += data[i];
    }
    
    return result;
}

/**
 * @brief AVX 向量点积
 */
inline double dotAVX(const double* a, const double* b, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 4);
    __m256d vsum = _mm256_setzero_pd();
    
    for (; i < simdCount; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        vsum = _mm256_add_pd(vsum, _mm256_mul_pd(va, vb));
    }
    
    // 水平求和
    __m128d vlow = _mm256_castpd256_pd128(vsum);
    __m128d vhigh = _mm256_extractf128_pd(vsum, 1);
    vlow = _mm_add_pd(vlow, vhigh);
    __m128d high64 = _mm_unpackhi_pd(vlow, vlow);
    double result = _mm_cvtsd_f64(_mm_add_sd(vlow, high64));
    
    for (; i < count; ++i) {
        result += a[i] * b[i];
    }
    
    return result;
}

#elif BT_SIMD_LEVEL >= 128  // SSE2

/**
 * @brief SSE2 向量加法
 */
inline void addSSE(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vb = _mm_loadu_pd(b + i);
        __m128d vr = _mm_add_pd(va, vb);
        _mm_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief SSE2 向量减法
 */
inline void subSSE(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vb = _mm_loadu_pd(b + i);
        __m128d vr = _mm_sub_pd(va, vb);
        _mm_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] - b[i];
    }
}

/**
 * @brief SSE2 向量乘法
 */
inline void mulSSE(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vb = _mm_loadu_pd(b + i);
        __m128d vr = _mm_mul_pd(va, vb);
        _mm_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief SSE2 向量除法
 */
inline void divSSE(const double* a, const double* b, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vb = _mm_loadu_pd(b + i);
        __m128d vr = _mm_div_pd(va, vb);
        _mm_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = (b[i] != 0) ? (a[i] / b[i]) : NaN;
    }
}

/**
 * @brief SSE2 标量乘法
 */
inline void mulScalarSSE(const double* a, double scalar, double* result, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    __m128d vs = _mm_set1_pd(scalar);
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vr = _mm_mul_pd(va, vs);
        _mm_storeu_pd(result + i, vr);
    }
    
    for (; i < count; ++i) {
        result[i] = a[i] * scalar;
    }
}

/**
 * @brief SSE2 向量求和
 */
inline double sumSSE(const double* data, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    __m128d vsum = _mm_setzero_pd();
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(data + i);
        vsum = _mm_add_pd(vsum, va);
    }
    
    // 水平求和
    __m128d high64 = _mm_unpackhi_pd(vsum, vsum);
    double result = _mm_cvtsd_f64(_mm_add_sd(vsum, high64));
    
    for (; i < count; ++i) {
        result += data[i];
    }
    
    return result;
}

/**
 * @brief SSE2 向量点积
 */
inline double dotSSE(const double* a, const double* b, Size count) {
    Size i = 0;
    Size simdCount = count - (count % 2);
    __m128d vsum = _mm_setzero_pd();
    
    for (; i < simdCount; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d vb = _mm_loadu_pd(b + i);
        vsum = _mm_add_pd(vsum, _mm_mul_pd(va, vb));
    }
    
    __m128d high64 = _mm_unpackhi_pd(vsum, vsum);
    double result = _mm_cvtsd_f64(_mm_add_sd(vsum, high64));
    
    for (; i < count; ++i) {
        result += a[i] * b[i];
    }
    
    return result;
}

#endif  // SIMD Level

// ============================================================================
// 标量实现（回退）
// ============================================================================

inline void addScalar(const double* a, const double* b, double* result, Size count) {
    for (Size i = 0; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

inline void subScalar(const double* a, const double* b, double* result, Size count) {
    for (Size i = 0; i < count; ++i) {
        result[i] = a[i] - b[i];
    }
}

inline void mulScalar(const double* a, const double* b, double* result, Size count) {
    for (Size i = 0; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
}

inline void divScalar(const double* a, const double* b, double* result, Size count) {
    for (Size i = 0; i < count; ++i) {
        result[i] = (b[i] != 0) ? (a[i] / b[i]) : NaN;
    }
}

inline void mulScalarVal(const double* a, double scalar, double* result, Size count) {
    for (Size i = 0; i < count; ++i) {
        result[i] = a[i] * scalar;
    }
}

inline double sumScalar(const double* data, Size count) {
    double result = 0;
    for (Size i = 0; i < count; ++i) {
        result += data[i];
    }
    return result;
}

inline double dotScalar(const double* a, const double* b, Size count) {
    double result = 0;
    for (Size i = 0; i < count; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

// ============================================================================
// 统一接口 - 自动选择最优实现
// ============================================================================

/**
 * @brief 向量加法 (自动选择最优实现)
 */
inline void add(const double* a, const double* b, double* result, Size count) {
#if BT_SIMD_LEVEL >= 256
    addAVX(a, b, result, count);
#elif BT_SIMD_LEVEL >= 128
    addSSE(a, b, result, count);
#else
    addScalar(a, b, result, count);
#endif
}

/**
 * @brief 向量减法 (自动选择最优实现)
 */
inline void sub(const double* a, const double* b, double* result, Size count) {
#if BT_SIMD_LEVEL >= 256
    subAVX(a, b, result, count);
#elif BT_SIMD_LEVEL >= 128
    subSSE(a, b, result, count);
#else
    subScalar(a, b, result, count);
#endif
}

/**
 * @brief 向量乘法 (自动选择最优实现)
 */
inline void mul(const double* a, const double* b, double* result, Size count) {
#if BT_SIMD_LEVEL >= 256
    mulAVX(a, b, result, count);
#elif BT_SIMD_LEVEL >= 128
    mulSSE(a, b, result, count);
#else
    mulScalar(a, b, result, count);
#endif
}

/**
 * @brief 向量除法 (自动选择最优实现)
 */
inline void div(const double* a, const double* b, double* result, Size count) {
#if BT_SIMD_LEVEL >= 256
    divAVX(a, b, result, count);
#elif BT_SIMD_LEVEL >= 128
    divSSE(a, b, result, count);
#else
    divScalar(a, b, result, count);
#endif
}

/**
 * @brief 标量乘法 (自动选择最优实现)
 */
inline void mulByScalar(const double* a, double scalar, double* result, Size count) {
#if BT_SIMD_LEVEL >= 256
    mulScalarAVX(a, scalar, result, count);
#elif BT_SIMD_LEVEL >= 128
    mulScalarSSE(a, scalar, result, count);
#else
    mulScalarVal(a, scalar, result, count);
#endif
}

/**
 * @brief 向量求和 (自动选择最优实现)
 */
inline double sum(const double* data, Size count) {
#if BT_SIMD_LEVEL >= 256
    return sumAVX(data, count);
#elif BT_SIMD_LEVEL >= 128
    return sumSSE(data, count);
#else
    return sumScalar(data, count);
#endif
}

/**
 * @brief 向量点积 (自动选择最优实现)
 */
inline double dot(const double* a, const double* b, Size count) {
#if BT_SIMD_LEVEL >= 256
    return dotAVX(a, b, count);
#elif BT_SIMD_LEVEL >= 128
    return dotSSE(a, b, count);
#else
    return dotScalar(a, b, count);
#endif
}

/**
 * @brief 向量求平均
 */
inline double mean(const double* data, Size count) {
    if (count == 0) return NaN;
    return sum(data, count) / static_cast<double>(count);
}

/**
 * @brief 向量求方差
 */
inline double variance(const double* data, Size count, double meanVal) {
    if (count < 2) return NaN;
    
    std::vector<double> diff(count);
    for (Size i = 0; i < count; ++i) {
        diff[i] = data[i] - meanVal;
    }
    
    double sumSq = dot(diff.data(), diff.data(), count);
    return sumSq / static_cast<double>(count);
}

/**
 * @brief 向量求标准差
 */
inline double stddev(const double* data, Size count, double meanVal) {
    double var = variance(data, count, meanVal);
    return std::isnan(var) ? NaN : std::sqrt(var);
}

/**
 * @brief 向量求最大值
 */
inline double max(const double* data, Size count) {
    if (count == 0) return NaN;
    double result = data[0];
    for (Size i = 1; i < count; ++i) {
        if (data[i] > result) result = data[i];
    }
    return result;
}

/**
 * @brief 向量求最小值
 */
inline double min(const double* data, Size count) {
    if (count == 0) return NaN;
    double result = data[0];
    for (Size i = 1; i < count; ++i) {
        if (data[i] < result) result = data[i];
    }
    return result;
}

// ============================================================================
// 高级向量运算
// ============================================================================

/**
 * @brief 滑动窗口求和（高效增量实现）
 */
inline void slidingSum(const double* data, double* result, Size dataLen, Size window) {
    if (dataLen == 0 || window == 0) return;
    
    // 前 window-1 个设为 NaN
    for (Size i = 0; i < window - 1 && i < dataLen; ++i) {
        result[i] = NaN;
    }
    
    if (dataLen < window) return;
    
    // 计算第一个窗口的和 (使用 SIMD)
    double windowSum = sum(data, window);
    result[window - 1] = windowSum;
    
    // 增量计算后续窗口
    for (Size i = window; i < dataLen; ++i) {
        windowSum = windowSum - data[i - window] + data[i];
        result[i] = windowSum;
    }
}

/**
 * @brief 滑动窗口求平均（SMA）
 */
inline void slidingMean(const double* data, double* result, Size dataLen, Size window) {
    slidingSum(data, result, dataLen, window);
    
    double divisor = static_cast<double>(window);
    for (Size i = window - 1; i < dataLen; ++i) {
        result[i] /= divisor;
    }
}

/**
 * @brief EMA 向量化计算
 */
inline void ema(const double* data, double* result, Size dataLen, Size period) {
    if (dataLen == 0 || period == 0) return;
    
    double alpha = 2.0 / (static_cast<double>(period) + 1.0);
    
    // 前 period-1 个设为 NaN
    for (Size i = 0; i < period - 1 && i < dataLen; ++i) {
        result[i] = NaN;
    }
    
    if (dataLen < period) return;
    
    // 用 SMA 初始化第一个值
    result[period - 1] = sum(data, period) / static_cast<double>(period);
    
    // 递推计算
    double oneMinusAlpha = 1.0 - alpha;
    for (Size i = period; i < dataLen; ++i) {
        result[i] = alpha * data[i] + oneMinusAlpha * result[i - 1];
    }
}

/**
 * @brief RSI 向量化计算
 */
inline void rsi(const double* data, double* result, Size dataLen, Size period) {
    if (dataLen < 2 || period == 0) {
        for (Size i = 0; i < dataLen; ++i) {
            result[i] = NaN;
        }
        return;
    }
    
    // 计算价格变化
    std::vector<double> changes(dataLen - 1);
    for (Size i = 0; i < dataLen - 1; ++i) {
        changes[i] = data[i + 1] - data[i];
    }
    
    // 分离正负变化
    std::vector<double> gains(dataLen - 1);
    std::vector<double> losses(dataLen - 1);
    for (Size i = 0; i < dataLen - 1; ++i) {
        gains[i] = changes[i] > 0 ? changes[i] : 0;
        losses[i] = changes[i] < 0 ? -changes[i] : 0;
    }
    
    // 计算 EMA
    std::vector<double> avgGain(dataLen - 1);
    std::vector<double> avgLoss(dataLen - 1);
    ema(gains.data(), avgGain.data(), dataLen - 1, period);
    ema(losses.data(), avgLoss.data(), dataLen - 1, period);
    
    // 第一个有效值在 period 位置 (因为计算变化丢失了一个点)
    for (Size i = 0; i < period; ++i) {
        result[i] = NaN;
    }
    
    // 计算 RSI
    for (Size i = period; i < dataLen; ++i) {
        Size idx = i - 1;  // 对应 changes/avgGain/avgLoss 的索引
        if (idx < period - 1) {
            result[i] = NaN;
            continue;
        }
        
        double ag = avgGain[idx];
        double al = avgLoss[idx];
        
        if (al == 0) {
            result[i] = 100.0;
        } else if (ag == 0) {
            result[i] = 0.0;
        } else {
            double rs = ag / al;
            result[i] = 100.0 - (100.0 / (1.0 + rs));
        }
    }
}

/**
 * @brief 布林带向量化计算
 */
inline void bollinger(const double* data, double* middle, double* upper, double* lower,
                      Size dataLen, Size period, double devFactor) {
    if (dataLen < period) {
        for (Size i = 0; i < dataLen; ++i) {
            middle[i] = upper[i] = lower[i] = NaN;
        }
        return;
    }
    
    // 计算中轨 (SMA)
    slidingMean(data, middle, dataLen, period);
    
    // 计算标准差并设置上下轨
    for (Size i = 0; i < period - 1; ++i) {
        upper[i] = lower[i] = NaN;
    }
    
    for (Size i = period - 1; i < dataLen; ++i) {
        // 计算该窗口的标准差
        const double* windowData = data + i - period + 1;
        double m = middle[i];
        double sd = stddev(windowData, period, m);
        
        upper[i] = m + devFactor * sd;
        lower[i] = m - devFactor * sd;
    }
}

/**
 * @brief MACD 向量化计算
 */
inline void macd(const double* data, double* macdLine, double* signalLine, double* histogram,
                 Size dataLen, Size fastPeriod, Size slowPeriod, Size signalPeriod) {
    if (dataLen < slowPeriod) {
        for (Size i = 0; i < dataLen; ++i) {
            macdLine[i] = signalLine[i] = histogram[i] = NaN;
        }
        return;
    }
    
    // 计算快慢 EMA
    std::vector<double> fastEMA(dataLen);
    std::vector<double> slowEMA(dataLen);
    
    ema(data, fastEMA.data(), dataLen, fastPeriod);
    ema(data, slowEMA.data(), dataLen, slowPeriod);
    
    // MACD Line = Fast EMA - Slow EMA
    for (Size i = 0; i < dataLen; ++i) {
        if (std::isnan(fastEMA[i]) || std::isnan(slowEMA[i])) {
            macdLine[i] = NaN;
        } else {
            macdLine[i] = fastEMA[i] - slowEMA[i];
        }
    }
    
    // 找到第一个有效的 MACD 值
    Size firstValid = 0;
    for (Size i = 0; i < dataLen; ++i) {
        if (!std::isnan(macdLine[i])) {
            firstValid = i;
            break;
        }
    }
    
    // Signal Line = MACD 的 EMA
    // 只对有效部分计算 EMA
    if (firstValid + signalPeriod <= dataLen) {
        std::vector<double> validMacd(dataLen - firstValid);
        std::vector<double> validSignal(dataLen - firstValid);
        
        for (Size i = firstValid; i < dataLen; ++i) {
            validMacd[i - firstValid] = macdLine[i];
        }
        
        ema(validMacd.data(), validSignal.data(), dataLen - firstValid, signalPeriod);
        
        for (Size i = 0; i < firstValid; ++i) {
            signalLine[i] = NaN;
        }
        for (Size i = firstValid; i < dataLen; ++i) {
            signalLine[i] = validSignal[i - firstValid];
        }
    } else {
        for (Size i = 0; i < dataLen; ++i) {
            signalLine[i] = NaN;
        }
    }
    
    // Histogram = MACD - Signal
    for (Size i = 0; i < dataLen; ++i) {
        if (std::isnan(macdLine[i]) || std::isnan(signalLine[i])) {
            histogram[i] = NaN;
        } else {
            histogram[i] = macdLine[i] - signalLine[i];
        }
    }
}

} // namespace simd
} // namespace bt
