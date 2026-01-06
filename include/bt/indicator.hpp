/**
 * @file indicator.hpp
 * @brief 指标基类 - 对应 Python 的 indicator.py
 * 
 * 所有技术指标的基类，支持：
 * - 声明输出线
 * - 参数系统
 * - 计算模式（once/next）
 * - 运算符重载生成新指标
 */

#pragma once

#include "bt/lineseries.hpp"
#include <functional>
#include <memory>

namespace bt {

// 前向声明
class Indicator;

/**
 * @brief 指标基类
 */
class Indicator : public LineSeries {
public:
    Indicator() = default;
    virtual ~Indicator() = default;
    
    /**
     * @brief 绑定数据源
     */
    void bindData(LineSeries* data) {
        data_ = data;
    }
    
    void bindData(LineBuffer* line) {
        singleLine_ = line;
    }
    
    /**
     * @brief 获取绑定的数据
     */
    LineSeries* data() { return data_; }
    const LineSeries* data() const { return data_; }
    
    /**
     * @brief 获取绑定的单线数据
     */
    LineBuffer* singleLineData() { return singleLine_; }
    const LineBuffer* singleLineData() const { return singleLine_; }
    
    /**
     * @brief 初始化（设置指标，创建子指标）
     * 子类应重写此方法
     */
    virtual void init() {}
    
    /**
     * @brief 逐 bar 计算（事件驱动模式）
     * 子类应重写此方法
     */
    virtual void next() {}
    
    /**
     * @brief 向量化计算（runonce 模式）
     * 默认实现循环调用 next()
     */
    virtual void once(Size start, Size end) {
        for (Size i = start; i < end; ++i) {
            next();
            advance();
        }
    }
    
    /**
     * @brief 预计算所有值（preload 模式）
     */
    void precompute() {
        if (!data_ && !singleLine_) return;
        
        Size len = data_ ? data_->line(0).length() : singleLine_->length();
        Size mp = minperiod();
        
        if (len < mp) return;
        
        home();
        once(0, len);
    }
    
    /**
     * @brief 获取输入数据的当前值
     */
    Value dataValue(Index idx = 0) const {
        if (singleLine_) {
            return (*singleLine_)[idx];
        }
        if (data_) {
            return (*data_)[idx];  // 默认使用第一条线
        }
        return NaN;
    }
    
    /**
     * @brief 获取输入数据的指定线
     */
    Value dataLine(Size lineIdx, Index idx = 0) const {
        if (data_) {
            return data_->line(lineIdx)[idx];
        }
        return NaN;
    }

protected:
    LineSeries* data_ = nullptr;
    LineBuffer* singleLine_ = nullptr;
};

/**
 * @brief 运算操作类型
 */
enum class OpType {
    Add, Sub, Mul, Div,
    Gt, Lt, Ge, Le, Eq, Ne,
    And, Or, Not,
    Max, Min, Abs, Neg
};

/**
 * @brief 线操作 - 两条线的运算
 */
class LineOp : public Indicator {
public:
    LineOp(LineBuffer* left, LineBuffer* right, OpType op)
        : left_(left), right_(right), op_(op), rightValue_(NaN) {
        addLine("result");
        Size mp = std::max(left->minperiod(), right->minperiod());
        setMinperiod(mp);
    }
    
    LineOp(LineBuffer* left, Value rightValue, OpType op)
        : left_(left), right_(nullptr), op_(op), rightValue_(rightValue) {
        addLine("result");
        setMinperiod(left->minperiod());
    }
    
    void next() override {
        Value l = (*left_)[0];
        Value r = right_ ? (*right_)[0] : rightValue_;
        Value result = compute(l, r);
        lines0().push(result);
    }
    
private:
    Value compute(Value l, Value r) const {
        switch (op_) {
            case OpType::Add: return l + r;
            case OpType::Sub: return l - r;
            case OpType::Mul: return l * r;
            case OpType::Div: return r != 0 ? l / r : NaN;
            case OpType::Gt:  return l > r ? 1.0 : 0.0;
            case OpType::Lt:  return l < r ? 1.0 : 0.0;
            case OpType::Ge:  return l >= r ? 1.0 : 0.0;
            case OpType::Le:  return l <= r ? 1.0 : 0.0;
            case OpType::Eq:  return l == r ? 1.0 : 0.0;
            case OpType::Ne:  return l != r ? 1.0 : 0.0;
            case OpType::And: return (l != 0 && r != 0) ? 1.0 : 0.0;
            case OpType::Or:  return (l != 0 || r != 0) ? 1.0 : 0.0;
            case OpType::Max: return std::max(l, r);
            case OpType::Min: return std::min(l, r);
            default: return NaN;
        }
    }
    
    LineBuffer* left_;
    LineBuffer* right_;
    OpType op_;
    Value rightValue_;
};

/**
 * @brief 单目运算
 */
class LineUnaryOp : public Indicator {
public:
    LineUnaryOp(LineBuffer* input, OpType op)
        : input_(input), op_(op) {
        addLine("result");
        setMinperiod(input->minperiod());
    }
    
    void next() override {
        Value v = (*input_)[0];
        Value result;
        switch (op_) {
            case OpType::Neg: result = -v; break;
            case OpType::Abs: result = std::abs(v); break;
            case OpType::Not: result = (v == 0) ? 1.0 : 0.0; break;
            default: result = NaN;
        }
        lines0().push(result);
    }
    
private:
    LineBuffer* input_;
    OpType op_;
};

// 智能指针工厂
using IndicatorPtr = std::shared_ptr<Indicator>;

template<typename T, typename... Args>
IndicatorPtr makeIndicator(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace bt
