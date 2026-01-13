/**
 * @file backtrader.hpp
 * @brief Backtrader C++ 主头文件
 * 
 * 包含所有核心功能的单一入口�?
 */

#pragma once

// 核心组件
#include "bt/core/common.hpp"
#include "bt/core/linebuffer.hpp"
#include "bt/core/lineseries.hpp"
#include "bt/core/params.hpp"
#include "bt/indicators/indicator.hpp"

// 引擎组件 (Phase 2)
#include "bt/broker/order.hpp"
#include "bt/feed/datafeed.hpp"
#include "bt/broker/broker.hpp"
#include "bt/strategy/strategy.hpp"
#include "bt/engine/cerebro.hpp"
#include "bt/analysis/analyzer.hpp"
#include "bt/analysis/observer.hpp"

// 高级特�?(Phase 3)
#include "bt/core/timeframe.hpp"
#include "bt/strategy/signal.hpp"
#include "bt/strategy/signalstrategy.hpp"
#include "bt/feed/resampler.hpp"

// 性能优化 (Phase 4)
#include "bt/core/vectorized.hpp"
#include "bt/core/simd.hpp"
#include "bt/core/threadpool.hpp"
#include "bt/engine/optimizer.hpp"

// 新增组件 (Phase 5 - Python 功能对齐)
#include "bt/broker/sizer.hpp"
#include "bt/broker/comminfo.hpp"
#include "bt/feed/filter.hpp"
#include "bt/core/timer.hpp"
#include "bt/analysis/writer.hpp"

// 指标
#include "bt/indicators/sma.hpp"
#include "bt/indicators/ema.hpp"
#include "bt/indicators/macd.hpp"
#include "bt/indicators/rsi.hpp"
#include "bt/indicators/bollinger.hpp"

namespace bt {

/**
 * @brief 版本字符�?
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
    // 静默实现，避�?iostream 依赖
}

} // namespace bt
