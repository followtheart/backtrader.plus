/**
 * @file linebuffer.hpp
 * @brief 核心时间序列缓冲区 - 对应 Python 的 linebuffer.py
 * 
 * LineBuffer 是整个框架的基础数据结构：
 * - 支持索引 [0] 表示"当前"值
 * - 支持 [1], [2]... 表示过去的值
 * - 支持 [-1], [-2]... 表示未来的值（仅在指标计算中）
 * - 支持两种内存模式：Unbounded（无界）和 QBuffer（固定大小）
 */

#pragma once

#include "bt/common.hpp"
#include <vector>
#include <deque>
#include <memory>
#include <stdexcept>
#include <algorithm>

namespace bt {

/**
 * @brief 缓冲区存储策略接口
 */
class IBufferStorage {
public:
    virtual ~IBufferStorage() = default;
    
    virtual void push_back(Value v) = 0;
    virtual Value& at(Index idx) = 0;
    virtual Value at(Index idx) const = 0;
    virtual Size size() const = 0;
    virtual void clear() = 0;
    virtual void reserve(Size n) = 0;
    
    // 支持前向/后向操作
    virtual void advance() = 0;      // 移动到下一个 bar
    virtual void rewind() = 0;       // 回退一个 bar
    virtual void home() = 0;         // 回到起点
    
    virtual Index position() const = 0;  // 当前位置
    virtual Size length() const = 0;     // 有效数据长度
};

/**
 * @brief 无界存储 - 保存所有历史数据
 */
class UnboundedStorage : public IBufferStorage {
public:
    UnboundedStorage() : pos_(0) {}
    
    void push_back(Value v) override {
        data_.push_back(v);
    }
    
    Value& at(Index idx) override {
        // idx=0 是当前, idx=1 是过去, idx=-1 是未来
        Index actual = pos_ - idx;
        if (actual < 0 || actual >= static_cast<Index>(data_.size())) {
            throw std::out_of_range("LineBuffer index out of range");
        }
        return data_[actual];
    }
    
    Value at(Index idx) const override {
        Index actual = pos_ - idx;
        if (actual < 0 || actual >= static_cast<Index>(data_.size())) {
            return NaN;  // 越界返回 NaN
        }
        return data_[actual];
    }
    
    Size size() const override { return data_.size(); }
    void clear() override { data_.clear(); pos_ = 0; }
    void reserve(Size n) override { data_.reserve(n); }
    
    void advance() override { 
        if (pos_ < static_cast<Index>(data_.size()) - 1) {
            ++pos_; 
        }
    }
    
    void rewind() override { 
        if (pos_ > 0) {
            --pos_; 
        }
    }
    
    void home() override { pos_ = 0; }
    
    Index position() const override { return pos_; }
    Size length() const override { return data_.size(); }
    
    // 获取原始数据（用于向量化计算）
    const std::vector<Value>& data() const { return data_; }
    std::vector<Value>& data() { return data_; }

private:
    std::vector<Value> data_;
    Index pos_;
};

/**
 * @brief 固定大小缓冲区 - 仅保存最近 N 个值（节省内存）
 */
class QBufferStorage : public IBufferStorage {
public:
    explicit QBufferStorage(Size maxlen) 
        : maxlen_(maxlen), pos_(0), total_pushed_(0) {}
    
    void push_back(Value v) override {
        if (data_.size() >= maxlen_) {
            data_.pop_front();
        }
        data_.push_back(v);
        ++total_pushed_;
    }
    
    Value& at(Index idx) override {
        Index actual = static_cast<Index>(data_.size()) - 1 - idx;
        if (actual < 0 || actual >= static_cast<Index>(data_.size())) {
            throw std::out_of_range("QBuffer index out of range");
        }
        return data_[actual];
    }
    
    Value at(Index idx) const override {
        Index actual = static_cast<Index>(data_.size()) - 1 - idx;
        if (actual < 0 || actual >= static_cast<Index>(data_.size())) {
            return NaN;
        }
        return data_[actual];
    }
    
    Size size() const override { return data_.size(); }
    void clear() override { data_.clear(); pos_ = 0; total_pushed_ = 0; }
    void reserve(Size) override { /* deque 不支持 reserve */ }
    
    void advance() override { ++pos_; }
    void rewind() override { if (pos_ > 0) --pos_; }
    void home() override { pos_ = 0; }
    
    Index position() const override { return pos_; }
    Size length() const override { return total_pushed_; }

private:
    std::deque<Value> data_;
    Size maxlen_;
    Index pos_;
    Size total_pushed_;
};

/**
 * @brief LineBuffer - 核心时间序列缓冲区
 * 
 * 对应 Python 版本的 backtrader.linebuffer.LineBuffer
 */
class LineBuffer {
public:
    /**
     * @brief 创建无界缓冲区
     */
    LineBuffer() 
        : storage_(std::make_unique<UnboundedStorage>())
        , minperiod_(1)
    {}
    
    /**
     * @brief 创建固定大小缓冲区
     * @param qbuffer 缓冲区大小
     */
    explicit LineBuffer(Size qbuffer)
        : storage_(std::make_unique<QBufferStorage>(qbuffer))
        , minperiod_(1)
    {}
    
    BT_DEFAULT_MOVE(LineBuffer)
    
    // 禁用拷贝（使用 clone() 方法）
    LineBuffer(const LineBuffer&) = delete;
    LineBuffer& operator=(const LineBuffer&) = delete;
    
    /**
     * @brief 索引操作符 - [0] 是当前值，[1] 是过去，[-1] 是未来
     */
    Value& operator[](Index idx) {
        return storage_->at(idx);
    }
    
    Value operator[](Index idx) const {
        return storage_->at(idx);
    }
    
    /**
     * @brief 添加新值
     */
    void push(Value v) {
        storage_->push_back(v);
    }
    
    /**
     * @brief 批量添加值
     */
    void extend(const std::vector<Value>& values) {
        for (Value v : values) {
            storage_->push_back(v);
        }
    }
    
    /**
     * @brief 移动到下一个 bar
     */
    void advance() { storage_->advance(); }
    
    /**
     * @brief 回退到上一个 bar
     */
    void rewind() { storage_->rewind(); }
    
    /**
     * @brief 回到起点
     */
    void home() { storage_->home(); }
    
    /**
     * @brief 获取当前位置
     */
    Index position() const { return storage_->position(); }
    
    /**
     * @brief 获取缓冲区长度
     */
    Size size() const { return storage_->size(); }
    
    /**
     * @brief 获取总数据长度
     */
    Size length() const { return storage_->length(); }
    
    /**
     * @brief 获取/设置最小周期
     */
    Size minperiod() const { return minperiod_; }
    void setMinperiod(Size mp) { minperiod_ = mp; }
    
    /**
     * @brief 更新最小周期（取最大值）
     */
    void updateMinperiod(Size mp) {
        if (mp > minperiod_) minperiod_ = mp;
    }
    
    /**
     * @brief 清空缓冲区
     */
    void reset() {
        storage_->clear();
    }
    
    /**
     * @brief 预分配空间
     */
    void reserve(Size n) {
        storage_->reserve(n);
    }
    
    /**
     * @brief 获取当前值的快捷方法
     */
    Value current() const { return (*this)[0]; }
    
    /**
     * @brief 检查是否有足够的数据
     */
    bool ready() const {
        return storage_->length() >= minperiod_;
    }
    
    /**
     * @brief 获取无界存储的原始数据（用于向量化）
     */
    std::vector<Value>* rawData() {
        auto* unbounded = dynamic_cast<UnboundedStorage*>(storage_.get());
        return unbounded ? &unbounded->data() : nullptr;
    }
    
    const std::vector<Value>* rawData() const {
        auto* unbounded = dynamic_cast<const UnboundedStorage*>(storage_.get());
        return unbounded ? &unbounded->data() : nullptr;
    }

private:
    std::unique_ptr<IBufferStorage> storage_;
    Size minperiod_;
};

/**
 * @brief 检查索引是否有效
 */
inline bool isValidIndex(const LineBuffer& buf, Index idx) {
    try {
        (void)buf[idx];
        return !isnan(buf[idx]);
    } catch (...) {
        return false;
    }
}

} // namespace bt
