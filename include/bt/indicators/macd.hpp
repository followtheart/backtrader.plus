/**
 * @file indicators/macd.hpp
 * @brief MACD 指标 (Moving Average Convergence Divergence)
 * 
 * 支持 SIMD 优化的向量化计算
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/indicators/ema.hpp"
#include "bt/simd.hpp"

namespace bt {
namespace indicators {

/**
 * @brief MACD 指标
 * 
 * 输出线:
 * - macd: 快线 EMA - 慢线 EMA
 * - signal: MACD 的 EMA
 * - histogram: MACD - Signal
 */
class MACD : public Indicator {
public:
    // 线索引
    static constexpr Size LINE_MACD = 0;
    static constexpr Size LINE_SIGNAL = 1;
    static constexpr Size LINE_HISTOGRAM = 2;
    
    BT_PARAMS_BEGIN()
        BT_PARAM(fast, 12)
        BT_PARAM(slow, 26)
        BT_PARAM(signal, 9)
    BT_PARAMS_END()
    
    explicit MACD(const Params& params = {}) {
        params_.override(params);
        setupLines();
    }
    
    MACD(LineBuffer* input, int fast = 12, int slow = 26, int signalPeriod = 9) {
        params_.set("fast", fast);
        params_.set("slow", slow);
        params_.set("signal", signalPeriod);
        bindData(input);
        setupLines();
    }
    
    MACD(LineSeries* data, int fast = 12, int slow = 26, int signalPeriod = 9) {
        params_.set("fast", fast);
        params_.set("slow", slow);
        params_.set("signal", signalPeriod);
        bindData(data);
        setupLines();
    }
    
    void init() override {
        int fast = p().get<int>("fast");
        int slow = p().get<int>("slow");
        int signalPeriod = p().get<int>("signal");
        
        LineBuffer* input = singleLine_ ? singleLine_ : &data_->line(0);
        
        emaFast_ = std::make_unique<EMA>(input, fast);
        emaSlow_ = std::make_unique<EMA>(input, slow);
        
        emaFast_->init();
        emaSlow_->init();
        
        // Signal 的 EMA 需要 MACD 线数据
        signalAlpha_ = 2.0 / (signalPeriod + 1);
        signalInitialized_ = false;
    }
    
    void next() override {
        emaFast_->next();
        emaSlow_->next();
        
        Value macdValue = emaFast_->value(0) - emaSlow_->value(0);
        line(LINE_MACD).push(macdValue);
        
        // 计算 Signal 线
        if (!signalInitialized_) {
            prevSignal_ = macdValue;
            signalInitialized_ = true;
        } else {
            prevSignal_ = signalAlpha_ * macdValue + (1.0 - signalAlpha_) * prevSignal_;
        }
        line(LINE_SIGNAL).push(prevSignal_);
        
        // 计算柱状图
        line(LINE_HISTOGRAM).push(macdValue - prevSignal_);
    }
    
    // 便捷访问器
    LineBuffer& macd() { return line(LINE_MACD); }
    LineBuffer& signal() { return line(LINE_SIGNAL); }
    LineBuffer& histogram() { return line(LINE_HISTOGRAM); }
    
    const LineBuffer& macd() const { return line(LINE_MACD); }
    const LineBuffer& signal() const { return line(LINE_SIGNAL); }
    const LineBuffer& histogram() const { return line(LINE_HISTOGRAM); }
    
    /**
     * @brief 向量化计算（SIMD 优化版本）
     */
    void once(Size start, Size end) override {
        int fast = p().get<int>("fast");
        int slow = p().get<int>("slow");
        int signalPeriod = p().get<int>("signal");
        
        const std::vector<Value>* rawInput = nullptr;
        if (singleLine_) {
            rawInput = singleLine_->rawData();
        } else if (data_) {
            rawInput = data_->line(0).rawData();
        }
        
        if (!rawInput) {
            Indicator::once(start, end);
            return;
        }
        
        // 使用 SIMD 优化的 MACD 计算
        Size len = rawInput->size();
        
        std::vector<Value> macdLine(len);
        std::vector<Value> signalLine(len);
        std::vector<Value> histLine(len);
        
        simd::macd(rawInput->data(), macdLine.data(), signalLine.data(), histLine.data(),
                   len, static_cast<Size>(fast), static_cast<Size>(slow), 
                   static_cast<Size>(signalPeriod));
        
        // 复制到输出线
        std::vector<Value>* rawMacd = line(LINE_MACD).rawData();
        std::vector<Value>* rawSignal = line(LINE_SIGNAL).rawData();
        std::vector<Value>* rawHist = line(LINE_HISTOGRAM).rawData();
        
        if (rawMacd && rawSignal && rawHist) {
            // 找到第一个有效值的位置
            Size validStart = 0;
            for (Size i = 0; i < len; ++i) {
                if (!std::isnan(histLine[i])) {
                    validStart = i;
                    break;
                }
            }
            
            Size validLen = len - validStart;
            rawMacd->resize(validLen);
            rawSignal->resize(validLen);
            rawHist->resize(validLen);
            
            for (Size i = validStart; i < len; ++i) {
                (*rawMacd)[i - validStart] = macdLine[i];
                (*rawSignal)[i - validStart] = signalLine[i];
                (*rawHist)[i - validStart] = histLine[i];
            }
        }
    }

private:
    void setupLines() {
        addLine("macd");
        addLine("signal");
        addLine("histogram");
        
        int slow = p().get<int>("slow");
        int signalPeriod = p().get<int>("signal");
        setMinperiod(slow + signalPeriod - 1);
    }
    
    std::unique_ptr<EMA> emaFast_;
    std::unique_ptr<EMA> emaSlow_;
    Value signalAlpha_ = 0.0;
    Value prevSignal_ = 0.0;
    bool signalInitialized_ = false;
};

} // namespace indicators

using MACD = indicators::MACD;

} // namespace bt
