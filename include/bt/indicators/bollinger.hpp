/**
 * @file indicators/bollinger.hpp
 * @brief 布林带指标 (Bollinger Bands)
 */

#pragma once

#include "bt/indicator.hpp"
#include "bt/indicators/sma.hpp"
#include <cmath>

namespace bt {
namespace indicators {

/**
 * @brief 标准差计算
 */
class StdDev : public Indicator {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 20)
    BT_PARAMS_END()
    
    explicit StdDev(const Params& params = {}) {
        params_.override(params);
        addLine("stddev");
        setMinperiod(p().get<int>("period"));
    }
    
    StdDev(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("stddev");
        setMinperiod(period);
    }
    
    void next() override {
        int period = p().get<int>("period");
        
        // 计算均值
        Value sum = 0.0;
        for (int i = 0; i < period; ++i) {
            sum += dataValue(i);
        }
        Value mean = sum / period;
        
        // 计算方差
        Value variance = 0.0;
        for (int i = 0; i < period; ++i) {
            Value diff = dataValue(i) - mean;
            variance += diff * diff;
        }
        variance /= period;
        
        lines0().push(std::sqrt(variance));
    }
    
    Value value(Index idx = 0) const { return lines0()[idx]; }
};

/**
 * @brief 布林带 (Bollinger Bands)
 * 
 * 输出线:
 * - mid: 中轨 (SMA)
 * - top: 上轨 (mid + devfactor * stddev)
 * - bot: 下轨 (mid - devfactor * stddev)
 */
class BollingerBands : public Indicator {
public:
    static constexpr Size LINE_MID = 0;
    static constexpr Size LINE_TOP = 1;
    static constexpr Size LINE_BOT = 2;
    
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 20)
        BT_PARAM(devfactor, 2.0)
    BT_PARAMS_END()
    
    explicit BollingerBands(const Params& params = {}) {
        params_.override(params);
        setupLines();
    }
    
    BollingerBands(LineBuffer* input, int period = 20, double devfactor = 2.0) {
        params_.set("period", period);
        params_.set("devfactor", devfactor);
        bindData(input);
        setupLines();
    }
    
    BollingerBands(LineSeries* data, int period = 20, double devfactor = 2.0) {
        params_.set("period", period);
        params_.set("devfactor", devfactor);
        bindData(data);
        setupLines();
    }
    
    void init() override {
        int period = p().get<int>("period");
        LineBuffer* input = singleLine_ ? singleLine_ : &data_->line(0);
        
        sma_ = std::make_unique<SMA>(input, period);
        stddev_ = std::make_unique<StdDev>(input, period);
    }
    
    void next() override {
        sma_->next();
        stddev_->next();
        
        double devfactor = p().get<double>("devfactor");
        
        Value mid = sma_->value(0);
        Value dev = stddev_->value(0);
        
        line(LINE_MID).push(mid);
        line(LINE_TOP).push(mid + devfactor * dev);
        line(LINE_BOT).push(mid - devfactor * dev);
    }
    
    // 便捷访问器
    LineBuffer& mid() { return line(LINE_MID); }
    LineBuffer& top() { return line(LINE_TOP); }
    LineBuffer& bot() { return line(LINE_BOT); }
    
    const LineBuffer& mid() const { return line(LINE_MID); }
    const LineBuffer& top() const { return line(LINE_TOP); }
    const LineBuffer& bot() const { return line(LINE_BOT); }
    
    // 判断价格位置
    bool isAboveTop(Value price, Index idx = 0) const {
        return price > top()[idx];
    }
    
    bool isBelowBot(Value price, Index idx = 0) const {
        return price < bot()[idx];
    }
    
    // 计算 %B (价格在布林带中的相对位置)
    Value percentB(Value price, Index idx = 0) const {
        Value t = top()[idx];
        Value b = bot()[idx];
        if (t == b) return 0.5;
        return (price - b) / (t - b);
    }
    
    // 计算带宽
    Value bandwidth(Index idx = 0) const {
        Value m = mid()[idx];
        if (m == 0) return 0.0;
        return (top()[idx] - bot()[idx]) / m;
    }

private:
    void setupLines() {
        addLine("mid");
        addLine("top");
        addLine("bot");
        setMinperiod(p().get<int>("period"));
    }
    
    std::unique_ptr<SMA> sma_;
    std::unique_ptr<StdDev> stddev_;
};

} // namespace indicators

using StdDev = indicators::StdDev;
using BollingerBands = indicators::BollingerBands;

} // namespace bt
