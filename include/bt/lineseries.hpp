/**
 * @file lineseries.hpp
 * @brief 多线对象容器 - 对应 Python 的 lineseries.py
 * 
 * LineSeries 是包含多个 LineBuffer 的容器：
 * - 数据源有 open, high, low, close, volume, openinterest 等线
 * - 指标可以有一条或多条输出线
 */

#pragma once

#include "bt/linebuffer.hpp"
#include "bt/params.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace bt {

/**
 * @brief 线信息结构
 */
struct LineInfo {
    std::string name;
    Size index;
};

/**
 * @brief LineSeries - 多线容器
 */
class LineSeries : public Parametrized<LineSeries> {
public:
    LineSeries() = default;
    
    explicit LineSeries(Size qbuffer) : qbuffer_(qbuffer) {}
    
    /**
     * @brief 添加一条新线
     */
    Size addLine(const std::string& name) {
        Size idx = lines_.size();
        if (qbuffer_ == UNBOUNDED) {
            lines_.push_back(std::make_unique<LineBuffer>());
        } else {
            lines_.push_back(std::make_unique<LineBuffer>(qbuffer_));
        }
        lineNames_[name] = idx;
        lineInfos_.push_back({name, idx});
        return idx;
    }
    
    /**
     * @brief 通过索引获取线
     */
    LineBuffer& line(Size idx) {
        BT_ASSERT(idx < lines_.size());
        return *lines_[idx];
    }
    
    const LineBuffer& line(Size idx) const {
        BT_ASSERT(idx < lines_.size());
        return *lines_[idx];
    }
    
    /**
     * @brief 通过名称获取线
     */
    LineBuffer& line(const std::string& name) {
        auto it = lineNames_.find(name);
        if (it == lineNames_.end()) {
            throw std::runtime_error("Line not found: " + name);
        }
        return *lines_[it->second];
    }
    
    const LineBuffer& line(const std::string& name) const {
        auto it = lineNames_.find(name);
        if (it == lineNames_.end()) {
            throw std::runtime_error("Line not found: " + name);
        }
        return *lines_[it->second];
    }
    
    /**
     * @brief 索引运算符 - 获取第一条线
     */
    Value& operator[](Index idx) {
        BT_ASSERT(!lines_.empty());
        return (*lines_[0])[idx];
    }
    
    Value operator[](Index idx) const {
        BT_ASSERT(!lines_.empty());
        return (*lines_[0])[idx];
    }
    
    /**
     * @brief 获取线数量
     */
    Size numLines() const { return lines_.size(); }
    
    /**
     * @brief 获取线信息
     */
    const std::vector<LineInfo>& lineInfos() const { return lineInfos_; }
    
    /**
     * @brief 检查是否有指定名称的线
     */
    bool hasLine(const std::string& name) const {
        return lineNames_.find(name) != lineNames_.end();
    }
    
    /**
     * @brief 获取第一条线（默认输出）
     */
    LineBuffer& lines0() {
        BT_ASSERT(!lines_.empty());
        return *lines_[0];
    }
    
    const LineBuffer& lines0() const {
        BT_ASSERT(!lines_.empty());
        return *lines_[0];
    }
    
    /**
     * @brief 移动所有线到下一个 bar
     */
    void advance() {
        for (auto& line : lines_) {
            line->advance();
        }
    }
    
    /**
     * @brief 回退所有线
     */
    void rewind() {
        for (auto& line : lines_) {
            line->rewind();
        }
    }
    
    /**
     * @brief 回到起点
     */
    void home() {
        for (auto& line : lines_) {
            line->home();
        }
    }
    
    /**
     * @brief 获取最小周期
     */
    Size minperiod() const {
        Size mp = 1;
        for (const auto& line : lines_) {
            mp = std::max(mp, line->minperiod());
        }
        return mp;
    }
    
    /**
     * @brief 设置所有线的最小周期
     */
    void setMinperiod(Size mp) {
        for (auto& line : lines_) {
            line->setMinperiod(mp);
        }
    }
    
    /**
     * @brief 更新最小周期
     */
    void updateMinperiod(Size mp) {
        for (auto& line : lines_) {
            line->updateMinperiod(mp);
        }
    }
    
    /**
     * @brief 检查是否所有线都准备好
     */
    bool ready() const {
        for (const auto& line : lines_) {
            if (!line->ready()) return false;
        }
        return true;
    }

protected:
    std::vector<std::unique_ptr<LineBuffer>> lines_;
    std::unordered_map<std::string, Size> lineNames_;
    std::vector<LineInfo> lineInfos_;
    Size qbuffer_ = UNBOUNDED;
};

/**
 * @brief OHLCV 数据 - 标准金融数据格式
 */
class OHLCVData : public LineSeries {
public:
    // 标准线索引
    static constexpr Size OPEN = 0;
    static constexpr Size HIGH = 1;
    static constexpr Size LOW = 2;
    static constexpr Size CLOSE = 3;
    static constexpr Size VOLUME = 4;
    static constexpr Size OPENINTEREST = 5;
    
    OHLCVData() {
        addLine("open");
        addLine("high");
        addLine("low");
        addLine("close");
        addLine("volume");
        addLine("openinterest");
    }
    
    // 便捷访问器
    LineBuffer& open() { return line(OPEN); }
    LineBuffer& high() { return line(HIGH); }
    LineBuffer& low() { return line(LOW); }
    LineBuffer& close() { return line(CLOSE); }
    LineBuffer& volume() { return line(VOLUME); }
    LineBuffer& openinterest() { return line(OPENINTEREST); }
    
    const LineBuffer& open() const { return line(OPEN); }
    const LineBuffer& high() const { return line(HIGH); }
    const LineBuffer& low() const { return line(LOW); }
    const LineBuffer& close() const { return line(CLOSE); }
    const LineBuffer& volume() const { return line(VOLUME); }
    const LineBuffer& openinterest() const { return line(OPENINTEREST); }
    
    /**
     * @brief 添加一个完整的 bar
     */
    void addBar(Value o, Value h, Value l, Value c, Value v = 0.0, Value oi = 0.0) {
        open().push(o);
        high().push(h);
        low().push(l);
        close().push(c);
        volume().push(v);
        openinterest().push(oi);
    }
};

} // namespace bt
