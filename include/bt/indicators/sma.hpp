/**
 * @file indicators/sma.hpp
 * @brief 简单移动平均 (Simple Moving Average)
 * 
 * 支持 SIMD 优化的向量化计算
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/simd.hpp"
#include <numeric>

namespace bt {
namespace indicators {

/**
 * @brief 简单移动平均 (SMA)
 * 
 * 计算公式: SMA = sum(close, period) / period
 */
class SMA : public Indicator {
public:
    // 参数定义
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 30)
    BT_PARAMS_END()
    
    explicit SMA(const Params& params = {}) {
        params_.override(params);
        addLine("sma");
        setMinperiod(p().get<int>("period"));
    }
    
    SMA(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("sma");
        setMinperiod(period);
    }
    
    SMA(LineSeries* data, int period) {
        params_.set("period", period);
        bindData(data);
        addLine("sma");
        setMinperiod(period);
    }
    
    void next() override {
        int period = p().get<int>("period");
        Value sum = 0.0;
        
        for (int i = 0; i < period; ++i) {
            sum += dataValue(i);
        }
        
        lines0().push(sum / period);
    }
    
    /**
     * @brief 向量化计算（SIMD 优化版本）
     */
    void once(Size start, Size end) override {
        int period = p().get<int>("period");
        
        // 获取原始输入数据
        const std::vector<Value>* rawInput = nullptr;
        if (singleLine_) {
            rawInput = singleLine_->rawData();
        } else if (data_) {
            rawInput = data_->line(0).rawData();
        }
        
        if (!rawInput || rawInput->empty()) {
            // 回退到逐 bar 计算
            Indicator::once(start, end);
            return;
        }
        
        Size len = rawInput->size();
        if (len < static_cast<Size>(period)) {
            return;
        }
        
        // 准备临时缓冲区
        std::vector<Value> tempOutput(len);
        
        // 使用 SIMD 优化的滑动窗口计算
        simd::slidingMean(rawInput->data(), tempOutput.data(), len, static_cast<Size>(period));
        
        // 只推入有效值（跳过前面的 NaN）
        for (Size i = static_cast<Size>(period) - 1; i < len; ++i) {
            lines0().push(tempOutput[i]);
        }
    }
    
    // 快捷方法获取当前值
    Value value(Index idx = 0) const { return lines0()[idx]; }
};

/**
 * @brief 加权移动平均 (WMA)
 */
class WMA : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 30)
    BT_PARAMS_END()
    
    explicit WMA(const Params& params = {}) {
        params_.override(params);
        addLine("wma");
        setMinperiod(p().get<int>("period"));
    }
    
    WMA(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("wma");
        setMinperiod(period);
    }
    
    void next() override {
        int period = p().get<int>("period");
        Value weightedSum = 0.0;
        Value weightTotal = 0.0;
        
        for (int i = 0; i < period; ++i) {
            Value weight = static_cast<Value>(period - i);
            weightedSum += dataValue(i) * weight;
            weightTotal += weight;
        }
        
        lines0().push(weightedSum / weightTotal);
    }
};

} // namespace indicators

// 别名
using SMA = indicators::SMA;
using WMA = indicators::WMA;

} // namespace bt
