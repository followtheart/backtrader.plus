/**
 * @file indicators/ema.hpp
 * @brief 指数移动平均 (Exponential Moving Average)
 * 
 * 支持 SIMD 优化的向量化计算
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/simd.hpp"

namespace bt {
namespace indicators {

/**
 * @brief 指数移动平均 (EMA)
 * 
 * 计算公式:
 * - 平滑因子: alpha = 2 / (period + 1)
 * - EMA[0] = alpha * data[0] + (1 - alpha) * EMA[1]
 * - 第一个值用 SMA 初始化
 */
class EMA : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 30)
    BT_PARAMS_END()
    
    explicit EMA(const Params& params = {}) {
        params_.override(params);
        addLine("ema");
        setMinperiod(p().get<int>("period"));
    }
    
    EMA(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("ema");
        setMinperiod(period);
    }
    
    EMA(LineSeries* data, int period) {
        params_.set("period", period);
        bindData(data);
        addLine("ema");
        setMinperiod(period);
    }
    
    void init() override {
        int period = p().get<int>("period");
        alpha_ = 2.0 / (period + 1);
        initialized_ = false;
    }
    
    void next() override {
        int period = p().get<int>("period");
        Value current = dataValue(0);
        
        if (!initialized_) {
            // 第一个值用 SMA 初始化
            Value sum = 0.0;
            for (int i = 0; i < period; ++i) {
                sum += dataValue(i);
            }
            prevEma_ = sum / period;
            initialized_ = true;
        }
        
        Value ema = alpha_ * current + (1.0 - alpha_) * prevEma_;
        prevEma_ = ema;
        lines0().push(ema);
    }
    
    void once(Size start, Size end) override {
        int period = p().get<int>("period");
        
        const std::vector<Value>* rawInput = nullptr;
        if (singleLine_) {
            rawInput = singleLine_->rawData();
        } else if (data_) {
            rawInput = data_->line(0).rawData();
        }
        
        std::vector<Value>* rawOutput = lines0().rawData();
        
        if (!rawInput || !rawOutput) {
            Indicator::once(start, end);
            return;
        }
        
        // 使用 SIMD 优化的 EMA 计算
        Size len = rawInput->size();
        rawOutput->resize(len);
        
        simd::ema(rawInput->data(), rawOutput->data(), len, static_cast<Size>(period));
        
        // 移除前面的 NaN 值
        Size validStart = period - 1;
        if (validStart > 0 && rawOutput->size() > len - validStart) {
            rawOutput->erase(rawOutput->begin(), rawOutput->begin() + validStart);
        }
    }
    
    Value value(Index idx = 0) const { return lines0()[idx]; }

private:
    Value alpha_ = 0.0;
    Value prevEma_ = 0.0;
    bool initialized_ = false;
};

/**
 * @brief 双指数移动平均 (DEMA)
 * DEMA = 2 * EMA - EMA(EMA)
 */
class DEMA : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 30)
    BT_PARAMS_END()
    
    explicit DEMA(const Params& params = {}) {
        params_.override(params);
        addLine("dema");
        int period = p().get<int>("period");
        // DEMA 需要 2 * period - 1 的预热期
        setMinperiod(2 * period - 1);
    }
    
    DEMA(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("dema");
        setMinperiod(2 * period - 1);
    }
    
    void init() override {
        int period = p().get<int>("period");
        ema1_ = std::make_unique<EMA>(singleLine_ ? singleLine_ : &data_->line(0), period);
        ema1_->init();
    }
    
    void next() override {
        // 需要两阶 EMA，先计算 EMA1
        ema1_->next();
        Value ema1Val = ema1_->value(0);
        
        // 计算 EMA2 (EMA of EMA)
        int period = p().get<int>("period");
        Value alpha = 2.0 / (period + 1);
        
        if (!ema2Initialized_) {
            // 初始化 EMA2
            ema2_ = ema1Val;
            ema2Initialized_ = true;
        } else {
            ema2_ = alpha * ema1Val + (1.0 - alpha) * ema2_;
        }
        
        // DEMA = 2 * EMA - EMA(EMA)
        Value dema = 2.0 * ema1Val - ema2_;
        lines0().push(dema);
    }

private:
    std::unique_ptr<EMA> ema1_;
    Value ema2_ = 0.0;
    bool ema2Initialized_ = false;
};

/**
 * @brief 三指数移动平均 (TEMA)
 * TEMA = 3 * EMA - 3 * EMA(EMA) + EMA(EMA(EMA))
 */
class TEMA : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 30)
    BT_PARAMS_END()
    
    explicit TEMA(const Params& params = {}) {
        params_.override(params);
        addLine("tema");
        int period = p().get<int>("period");
        setMinperiod(3 * period - 2);
    }
    
    void next() override {
        // 简化实现，实际需要三阶 EMA
        // TODO: 完整实现
        lines0().push(dataValue(0));
    }
};

} // namespace indicators

using EMA = indicators::EMA;
using DEMA = indicators::DEMA;
using TEMA = indicators::TEMA;

} // namespace bt
