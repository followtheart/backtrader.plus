/**
 * @file backtrader.hpp
 * @brief Backtrader C++ 主头文件
 * 
 * 包含所有核心功能的单一入口点
 */

#pragma once

// 核心组件
#include "bt/common.hpp"
#include "bt/linebuffer.hpp"
#include "bt/lineseries.hpp"
#include "bt/params.hpp"
#include "bt/indicator.hpp"

// 引擎组件 (Phase 2)
#include "bt/order.hpp"
#include "bt/datafeed.hpp"
#include "bt/broker.hpp"
#include "bt/strategy.hpp"
#include "bt/cerebro.hpp"
#include "bt/analyzer.hpp"
#include "bt/observer.hpp"

// 高级特性 (Phase 3)
#include "bt/timeframe.hpp"
#include "bt/signal.hpp"
#include "bt/signalstrategy.hpp"
#include "bt/resampler.hpp"

// 性能优化 (Phase 4)
#include "bt/vectorized.hpp"
#include "bt/simd.hpp"
#include "bt/threadpool.hpp"
#include "bt/optimizer.hpp"

// 新增组件 (Phase 5 - Python 功能对齐)
#include "bt/sizer.hpp"
#include "bt/comminfo.hpp"
#include "bt/filter.hpp"
#include "bt/timer.hpp"
#include "bt/writer.hpp"

// 指标
#include "bt/indicators/sma.hpp"
#include "bt/indicators/ema.hpp"
#include "bt/indicators/macd.hpp"
#include "bt/indicators/rsi.hpp"
#include "bt/indicators/bollinger.hpp"

namespace bt {

/**
 * @brief 版本字符串
 */
inline const char* version() {
    return "0.4.0";  // Phase 4
}

/**
 * @brief 获取 SIMD 支持信息
 */
inline const char* simdInfo() {
    return simd::getSIMDLevel();
}

/**
 * @brief 打印版本信息
 */
inline void printVersion() {
    // 静默实现，避免 iostream 依赖
}

} // namespace bt
