/**
 * @file indicators/rsi.hpp
 * @brief RSI 相对强弱指标 (Relative Strength Index)
 * 
 * 支持 SIMD 优化的向量化计算
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/simd.hpp"

namespace bt {
namespace indicators {

/**
 * @brief RSI 相对强弱指标
 * 
 * 计算公式:
 * - RS = 平均涨幅 / 平均跌幅
 * - RSI = 100 - 100 / (1 + RS)
 * 
 * 使用 Wilder 平滑方法（等效于 alpha = 1/period 的 EMA）
 */
class RSI : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 14)
        BT_PARAM(upperband, 70.0)
        BT_PARAM(lowerband, 30.0)
    BT_PARAMS_END()
    
    explicit RSI(const Params& params = {}) {
        params_.override(params);
        setupLines();
    }
    
    RSI(LineBuffer* input, int period = 14) {
        params_.set("period", period);
        bindData(input);
        setupLines();
    }
    
    RSI(LineSeries* data, int period = 14) {
        params_.set("period", period);
        bindData(data);
        setupLines();
    }
    
    void init() override {
        int period = p().get<int>("period");
        alpha_ = 1.0 / period;
        avgGain_ = 0.0;
        avgLoss_ = 0.0;
        initialized_ = false;
        barCount_ = 0;
    }
    
    void next() override {
        int period = p().get<int>("period");
        Value current = dataValue(0);
        Value prev = dataValue(1);
        
        Value change = current - prev;
        Value gain = change > 0 ? change : 0.0;
        Value loss = change < 0 ? -change : 0.0;
        
        ++barCount_;
        
        if (!initialized_) {
            // 累加前 period 个值
            sumGain_ += gain;
            sumLoss_ += loss;
            
            if (barCount_ >= static_cast<Size>(period)) {
                avgGain_ = sumGain_ / period;
                avgLoss_ = sumLoss_ / period;
                initialized_ = true;
            } else {
                lines0().push(NaN);
                return;
            }
        } else {
            // Wilder 平滑
            avgGain_ = alpha_ * gain + (1.0 - alpha_) * avgGain_;
            avgLoss_ = alpha_ * loss + (1.0 - alpha_) * avgLoss_;
        }
        
        Value rsi;
        if (avgLoss_ == 0.0) {
            rsi = 100.0;
        } else {
            Value rs = avgGain_ / avgLoss_;
            rsi = 100.0 - 100.0 / (1.0 + rs);
        }
        
        lines0().push(rsi);
    }
    
    Value value(Index idx = 0) const { return lines0()[idx]; }
    
    // 判断超买超卖
    bool isOverbought(Index idx = 0) const {
        return value(idx) > p().get<double>("upperband");
    }
    
    bool isOversold(Index idx = 0) const {
        return value(idx) < p().get<double>("lowerband");
    }
    
    /**
     * @brief 向量化计算（SIMD 优化版本）
     */
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
        
        // 使用 SIMD 优化的 RSI 计算
        Size len = rawInput->size();
        rawOutput->resize(len);
        
        simd::rsi(rawInput->data(), rawOutput->data(), len, static_cast<Size>(period));
        
        // 移除前面的 NaN 值
        Size validStart = period;
        if (validStart > 0 && rawOutput->size() > len - validStart) {
            rawOutput->erase(rawOutput->begin(), rawOutput->begin() + validStart);
        }
    }

private:
    void setupLines() {
        addLine("rsi");
        setMinperiod(p().get<int>("period") + 1);  // 需要前一个值计算变化
    }
    
    Value alpha_ = 0.0;
    Value avgGain_ = 0.0;
    Value avgLoss_ = 0.0;
    Value sumGain_ = 0.0;
    Value sumLoss_ = 0.0;
    bool initialized_ = false;
    Size barCount_ = 0;
};

/**
 * @brief Stochastic RSI
 */
class StochRSI : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 14)
        BT_PARAM(rsiperiod, 14)
    BT_PARAMS_END()
    
    explicit StochRSI(const Params& params = {}) {
        params_.override(params);
        addLine("stochrsi");
        int period = p().get<int>("period");
        int rsiperiod = p().get<int>("rsiperiod");
        setMinperiod(period + rsiperiod);
    }
    
    void init() override {
        int rsiperiod = p().get<int>("rsiperiod");
        LineBuffer* input = singleLine_ ? singleLine_ : &data_->line(0);
        rsi_ = std::make_unique<RSI>(input, rsiperiod);
        rsi_->init();
    }
    
    void next() override {
        int period = p().get<int>("period");
        rsi_->next();
        
        // 找最近 period 个 RSI 的最高和最低
        Value rsiVal = rsi_->value(0);
        Value highest = rsiVal;
        Value lowest = rsiVal;
        
        for (int i = 1; i < period; ++i) {
            Value v = rsi_->value(i);
            if (!isnan(v)) {
                highest = std::max(highest, v);
                lowest = std::min(lowest, v);
            }
        }
        
        Value stochRsi;
        if (highest == lowest) {
            stochRsi = 0.5;  // 避免除零
        } else {
            stochRsi = (rsiVal - lowest) / (highest - lowest);
        }
        
        lines0().push(stochRsi);
    }

private:
    std::unique_ptr<RSI> rsi_;
};

} // namespace indicators

using RSI = indicators::RSI;
using StochRSI = indicators::StochRSI;

} // namespace bt
