# Backtrader C++ 自定义策略开发教程

本教程面向二次开发人员，详细介绍如何基于 Backtrader C++ 框架开发自定义交易策略。

## 目录

1. [快速入门](#1-快速入门)
2. [核心概念](#2-核心概念)
3. [Strategy 类详解](#3-strategy-类详解)
4. [数据访问](#4-数据访问)
5. [交易操作](#5-交易操作)
6. [使用技术指标](#6-使用技术指标)
7. [自定义指标开发](#7-自定义指标开发)
8. [订单与通知](#8-订单与通知)
9. [参数化策略](#9-参数化策略)
10. [完整策略示例](#10-完整策略示例)
11. [Python 迁移指南](#11-python-迁移指南)
12. [调试技巧](#12-调试技巧)
13. [性能优化](#13-性能优化)

---

## 1. 快速入门

### 1.1 最小策略示例

```cpp
#include "bt/backtrader.hpp"
#include <iostream>

// 定义策略类，继承自 bt::Strategy
class MyFirstStrategy : public bt::Strategy {
public:
    void next() override {
        // 获取当前收盘价
        double price = data(0)->close()[0];
        
        // 简单逻辑：如果没有持仓且价格上涨，则买入
        if (position() == 0 && data(0)->close()[0] > data(0)->close()[1]) {
            buy();
            std::cout << "买入信号 @ " << price << std::endl;
        }
    }
};

int main() {
    bt::Cerebro cerebro;
    
    // 添加数据
    auto data = std::make_shared<bt::MemoryDataFeed>();
    // ... 添加价格数据 ...
    cerebro.addData(data);
    
    // 添加策略
    cerebro.addStrategy<MyFirstStrategy>();
    
    // 设置初始资金
    cerebro.setCash(100000.0);
    
    // 运行回测
    auto results = cerebro.run();
    
    return 0;
}
```

### 1.2 编译配置

确保 CMakeLists.txt 中链接 backtrader_core：

```cmake
add_executable(my_strategy main.cpp)
target_link_libraries(my_strategy PRIVATE backtrader_core)
target_include_directories(my_strategy PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

---

## 2. 核心概念

### 2.1 架构概览

```
Cerebro (回测引擎)
    ├── DataFeed (数据源) → LineSeries → LineBuffer
    ├── Strategy (策略) 
    │   ├── 生命周期方法: init() → start() → prenext() → nextstart() → next() → stop()
    │   ├── 交易方法: buy(), sell(), closePosition()
    │   └── 通知回调: notifyOrder(), notifyTrade()
    ├── Broker (经纪商) → Order → Trade
    └── Analyzer (分析器)
```

### 2.2 数据索引规则（重要！）

**与 Python Backtrader 的关键区别**：

| 位置 | Python Backtrader | C++ Backtrader |
|------|-------------------|----------------|
| 当前值 | `self.data.close[0]` | `data(0)->close()[0]` |
| 前一个值 | `self.data.close[-1]` | `data(0)->close()[1]` |
| 前两个值 | `self.data.close[-2]` | `data(0)->close()[2]` |
| 未来值（仅指标） | `self.data.close[1]` | `data(0)->close()[-1]` |

**规则总结**：
- `[0]` = 当前 bar
- `[1], [2], ...` = 过去 1、2... 个 bar（正数表示过去）
- `[-1], [-2], ...` = 未来 bar（仅在指标计算中使用）

---

## 3. Strategy 类详解

### 3.1 生命周期方法

```cpp
class MyStrategy : public bt::Strategy {
public:
    /**
     * 初始化阶段 - 设置指标、参数
     * 对应 Python 的 __init__
     */
    void init() override {
        // 设置最小周期（等待指标预热）
        setMinPeriod(20);
        
        // 在此创建指标
    }
    
    /**
     * 回测开始前调用
     */
    void start() override {
        std::cout << "回测开始，初始资金: " << getBroker()->getCash() << std::endl;
    }
    
    /**
     * 指标预热期调用（数据不足时）
     */
    void prenext() override {
        // 可选：在预热期执行某些操作
    }
    
    /**
     * 指标就绪后首次调用（仅一次）
     */
    void nextstart() override {
        std::cout << "指标预热完成，开始正式交易" << std::endl;
        next();  // 默认行为是调用 next()
    }
    
    /**
     * 核心交易逻辑 - 每个 bar 调用一次
     */
    void next() override {
        // 在此编写交易策略
    }
    
    /**
     * 回测结束后调用
     */
    void stop() override {
        std::cout << "回测结束" << std::endl;
    }
};
```

### 3.2 Cheat-On-Open 模式

如需在开盘价执行订单：

```cpp
class CheatStrategy : public bt::Strategy {
public:
    // 在 bar 开始时调用（使用开盘价）
    void nextOpen() override {
        if (condition) {
            buy();  // 以开盘价执行
        }
    }
};
```

启用 Cheat-On-Open：
```cpp
cerebro.p().set("cheat_on_open", true);
```

---

## 4. 数据访问

### 4.1 访问 OHLCV 数据

```cpp
void next() override {
    // 获取第一个数据源（默认）
    DataFeed* d = data(0);
    
    // 访问 OHLCV 数据
    bt::Value open   = d->open()[0];    // 当前开盘价
    bt::Value high   = d->high()[0];    // 当前最高价
    bt::Value low    = d->low()[0];     // 当前最低价
    bt::Value close  = d->close()[0];   // 当前收盘价
    bt::Value volume = d->volume()[0];  // 当前成交量
    
    // 访问历史数据
    bt::Value prevClose = d->close()[1];   // 前一个收盘价
    bt::Value prev2Close = d->close()[2];  // 前两个收盘价
    
    // 检查数据有效性
    if (bt::isnan(close)) {
        return;  // 数据无效，跳过
    }
}
```

### 4.2 多数据源

```cpp
void init() override {
    // 数据源通过索引访问
    // data(0) - 第一个数据源
    // data(1) - 第二个数据源
    // ...
}

void next() override {
    // 访问不同数据源
    bt::Value price1 = data(0)->close()[0];
    bt::Value price2 = data(1)->close()[0];
    
    // 计算价差
    bt::Value spread = price1 - price2;
}
```

### 4.3 数据长度检查

```cpp
void next() override {
    // 检查是否有足够的历史数据
    if (data(0)->close().length() < 20) {
        return;  // 数据不足，等待
    }
    
    // 安全访问过去 20 个 bar
    for (int i = 0; i < 20; ++i) {
        bt::Value price = data(0)->close()[i];
    }
}
```

---

## 5. 交易操作

### 5.1 基础订单

```cpp
void next() override {
    // 市价买入（使用默认仓位大小）
    buy();
    
    // 指定数量买入
    buy(nullptr, 100);  // 买入 100 股
    
    // 限价买入
    buy(nullptr, 100, 50.0, bt::OrderType::Limit);
    
    // 市价卖出
    sell();
    
    // 指定数量卖出
    sell(nullptr, 100);
    
    // 平仓
    closePosition();
}
```

### 5.2 目标订单

```cpp
void next() override {
    bt::Value currentPrice = data(0)->close()[0];
    
    // 目标持仓数量
    orderTargetSize(nullptr, 100);    // 调整到 100 股
    orderTargetSize(nullptr, -50);    // 调整到 -50 股（空头）
    
    // 目标持仓价值
    orderTargetValue(nullptr, 10000); // 调整到价值 $10000
    
    // 目标持仓百分比（占总资产）
    orderTargetPercent(nullptr, 50);  // 调整到占 50% 仓位
}
```

### 5.3 获取持仓信息

```cpp
void next() override {
    // 获取当前持仓
    bt::Value pos = position();  // 返回持仓数量
    
    // 或者指定数据源
    bt::Value pos = getPosition(data(0));
    
    // 根据持仓方向做决策
    if (pos > 0) {
        // 多头持仓
    } else if (pos < 0) {
        // 空头持仓
    } else {
        // 空仓
    }
}
```

### 5.4 Bracket 订单（止盈止损）

```cpp
void next() override {
    bt::Strategy::BracketConfig config;
    config.size = 100;
    config.price = 100.0;       // 入场价
    config.stopPrice = 95.0;    // 止损价
    config.limitPrice = 110.0;  // 止盈价
    
    // 创建 bracket 买单
    auto [mainOrder, stopOrder, limitOrder] = buyBracket(nullptr, config);
}
```

### 5.5 取消订单

```cpp
// 保存订单引用
Order* pendingOrder = nullptr;

void next() override {
    if (condition) {
        pendingOrder = buy();
    }
    
    // 取消订单
    if (pendingOrder && shouldCancel) {
        cancel(pendingOrder);
        pendingOrder = nullptr;
    }
}
```

---

## 6. 使用技术指标

### 6.1 内置指标

```cpp
#include "bt/backtrader.hpp"

class IndicatorStrategy : public bt::Strategy {
private:
    bt::SMA* smaFast_ = nullptr;
    bt::SMA* smaSlow_ = nullptr;
    bt::RSI* rsi_ = nullptr;
    bt::MACD* macd_ = nullptr;
    bt::BollingerBands* bb_ = nullptr;
    
public:
    void init() override {
        // 创建 SMA 指标
        smaFast_ = new bt::SMA(&data(0)->close(), 10);
        smaSlow_ = new bt::SMA(&data(0)->close(), 30);
        
        // 创建 RSI 指标
        rsi_ = new bt::RSI(&data(0)->close(), 14);
        
        // 创建 MACD 指标
        macd_ = new bt::MACD(&data(0)->close(), 12, 26, 9);
        
        // 创建布林带指标
        bb_ = new bt::BollingerBands(&data(0)->close(), 20, 2.0);
        
        // 设置最小周期
        setMinPeriod(30);
    }
    
    void next() override {
        // 使用指标值
        bt::Value fastSMA = smaFast_->value();
        bt::Value slowSMA = smaSlow_->value();
        bt::Value rsiValue = rsi_->value();
        bt::Value macdLine = macd_->macd()[0];
        bt::Value signalLine = macd_->signal()[0];
        bt::Value bbMid = bb_->mid()[0];
        bt::Value bbTop = bb_->top()[0];
        bt::Value bbBot = bb_->bot()[0];
        
        // 金叉买入
        if (smaFast_->value(1) <= smaSlow_->value(1) && 
            fastSMA > slowSMA && 
            position() == 0) {
            buy();
        }
        
        // RSI 超卖买入
        if (rsiValue < 30 && position() == 0) {
            buy();
        }
    }
    
    ~IndicatorStrategy() {
        delete smaFast_;
        delete smaSlow_;
        delete rsi_;
        delete macd_;
        delete bb_;
    }
};
```

### 6.2 指标参数

```cpp
// 通过参数对象创建
bt::Params params;
params.set("period", 20);
bt::SMA sma(params);
sma.bindData(&data(0)->close());

// 直接构造
bt::SMA sma(&data(0)->close(), 20);
```

---

## 7. 自定义指标开发

### 7.1 基础指标模板

```cpp
#pragma once
#include "bt/indicator.hpp"

namespace bt {
namespace indicators {

/**
 * @brief 自定义动量指标
 * 
 * 计算: Momentum = Close[0] - Close[n]
 */
class Momentum : public Indicator {
public:
    // 参数定义
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 10)
    BT_PARAMS_END()
    
    explicit Momentum(const Params& params = {}) {
        params_.override(params);
        addLine("momentum");  // 添加输出线
        setMinperiod(p().get<int>("period") + 1);
    }
    
    Momentum(LineBuffer* input, int period) {
        params_.set("period", period);
        bindData(input);
        addLine("momentum");
        setMinperiod(period + 1);
    }
    
    // 逐 bar 计算
    void next() override {
        int period = p().get<int>("period");
        Value current = dataValue(0);
        Value past = dataValue(period);
        lines0().push(current - past);
    }
    
    // 可选：向量化计算（提升性能）
    void once(Size start, Size end) override {
        int period = p().get<int>("period");
        const auto* rawInput = singleLine_ ? singleLine_->rawData() : nullptr;
        
        if (!rawInput) {
            Indicator::once(start, end);  // 回退到逐 bar 计算
            return;
        }
        
        for (Size i = period; i < rawInput->size(); ++i) {
            lines0().push((*rawInput)[i] - (*rawInput)[i - period]);
        }
    }
    
    // 便捷访问方法
    Value value(Index idx = 0) const { return lines0()[idx]; }
};

} // namespace indicators
} // namespace bt
```

### 7.2 多输出线指标

```cpp
class MyMultiLineIndicator : public Indicator {
public:
    MyMultiLineIndicator() {
        addLine("upper");   // 索引 0
        addLine("middle");  // 索引 1
        addLine("lower");   // 索引 2
    }
    
    void next() override {
        Value mid = dataValue(0);
        Value upper = mid + 10;
        Value lower = mid - 10;
        
        line(0).push(upper);   // upper 线
        line(1).push(mid);     // middle 线
        line(2).push(lower);   // lower 线
    }
    
    // 便捷访问器
    LineBuffer& upper() { return line(0); }
    LineBuffer& middle() { return line(1); }
    LineBuffer& lower() { return line(2); }
};
```

### 7.3 组合指标

```cpp
class RSIWithMA : public Indicator {
private:
    std::unique_ptr<RSI> rsi_;
    std::unique_ptr<SMA> rsiMA_;
    
public:
    RSIWithMA(LineBuffer* input, int rsiPeriod = 14, int maPeriod = 5) {
        bindData(input);
        addLine("rsi");
        addLine("rsi_ma");
        
        // 创建子指标
        rsi_ = std::make_unique<RSI>(input, rsiPeriod);
        rsiMA_ = std::make_unique<SMA>(&rsi_->lines0(), maPeriod);
        
        setMinperiod(rsiPeriod + maPeriod);
    }
    
    void next() override {
        rsi_->next();
        rsiMA_->next();
        
        line(0).push(rsi_->value());
        line(1).push(rsiMA_->value());
    }
};
```

---

## 8. 订单与通知

### 8.1 订单状态回调

```cpp
class NotifyStrategy : public bt::Strategy {
public:
    void notifyOrder(Order& order) override {
        switch (order.status()) {
            case OrderStatus::Submitted:
                std::cout << "订单已提交" << std::endl;
                break;
                
            case OrderStatus::Accepted:
                std::cout << "订单已接受" << std::endl;
                break;
                
            case OrderStatus::Completed:
                std::cout << "订单已成交"
                          << " 价格: " << order.executedPrice()
                          << " 数量: " << order.executedSize()
                          << std::endl;
                break;
                
            case OrderStatus::Canceled:
                std::cout << "订单已取消" << std::endl;
                break;
                
            case OrderStatus::Rejected:
                std::cout << "订单被拒绝" << std::endl;
                break;
                
            case OrderStatus::Margin:
                std::cout << "保证金不足" << std::endl;
                break;
                
            default:
                break;
        }
    }
};
```

### 8.2 交易回调

```cpp
void notifyTrade(Trade& trade) override {
    if (trade.isOpen) {
        std::cout << "开仓: " 
                  << " 方向: " << (trade.size > 0 ? "多" : "空")
                  << " 价格: " << trade.price
                  << " 数量: " << std::abs(trade.size)
                  << std::endl;
    } else {
        std::cout << "平仓:"
                  << " 盈亏: " << trade.pnl
                  << " 盈亏(含手续费): " << trade.pnlComm
                  << std::endl;
    }
}
```

### 8.3 资金通知

```cpp
void notifyCashValue(Value cash, Value value) override {
    std::cout << "当前现金: " << cash 
              << " 总资产: " << value 
              << std::endl;
}
```

---

## 9. 参数化策略

### 9.1 使用参数宏

```cpp
class ParameterizedStrategy : public bt::Strategy {
public:
    // 使用宏定义参数及默认值
    BT_PARAMS_BEGIN()
        BT_PARAM(fastPeriod, 10)
        BT_PARAM(slowPeriod, 30)
        BT_PARAM(stopLoss, 0.02)
        BT_PARAM(takeProfit, 0.05)
    BT_PARAMS_END()
    
    void init() override {
        // 通过 p() 访问参数
        int fast = p().get<int>("fastPeriod");
        int slow = p().get<int>("slowPeriod");
        double sl = p().get<double>("stopLoss");
        
        setMinPeriod(slow);
    }
    
    void next() override {
        double stopLoss = p().get<double>("stopLoss");
        // 使用参数...
    }
};
```

### 9.2 构造函数传参

```cpp
class ConfigurableStrategy : public bt::Strategy {
public:
    ConfigurableStrategy(int fastPeriod = 10, int slowPeriod = 30)
        : fastPeriod_(fastPeriod), slowPeriod_(slowPeriod) {}
    
    void init() override {
        setMinPeriod(slowPeriod_);
    }
    
private:
    int fastPeriod_;
    int slowPeriod_;
};

// 使用
cerebro.addStrategy<ConfigurableStrategy>(5, 20);
```

### 9.3 参数优化

```cpp
// 使用 Optimizer 进行参数优化
bt::Optimizer optimizer(cerebro);

// 定义参数范围
optimizer.addParamRange("fastPeriod", 5, 20, 5);   // 5, 10, 15, 20
optimizer.addParamRange("slowPeriod", 20, 50, 10); // 20, 30, 40, 50

// 运行优化
auto results = optimizer.run();

// 获取最优参数
auto best = optimizer.getBest();
```

---

## 10. 完整策略示例

### 10.1 SMA 交叉策略

```cpp
#include "bt/backtrader.hpp"
#include <iostream>
#include <iomanip>

class SMACrossover : public bt::Strategy {
public:
    SMACrossover(int fastPeriod = 10, int slowPeriod = 30)
        : fastPeriod_(fastPeriod), slowPeriod_(slowPeriod) {}
    
    void init() override {
        setMinPeriod(slowPeriod_);
    }
    
    void next() override {
        if (!data(0)) return;
        
        auto& close = data(0)->close();
        if (close.length() < slowPeriod_) return;
        
        // 计算 SMA
        double fastSMA = calculateSMA(close, fastPeriod_);
        double slowSMA = calculateSMA(close, slowPeriod_);
        double prevFastSMA = calculateSMA(close, fastPeriod_, 1);
        double prevSlowSMA = calculateSMA(close, slowPeriod_, 1);
        
        // 检测金叉/死叉
        bool goldenCross = prevFastSMA <= prevSlowSMA && fastSMA > slowSMA;
        bool deathCross = prevFastSMA >= prevSlowSMA && fastSMA < slowSMA;
        
        bt::Value pos = position();
        
        if (goldenCross && pos <= 0) {
            if (pos < 0) closePosition();
            buy();
            std::cout << "金叉买入 @ " << close[0] << std::endl;
        }
        else if (deathCross && pos >= 0) {
            if (pos > 0) closePosition();
            sell();
            std::cout << "死叉卖出 @ " << close[0] << std::endl;
        }
    }
    
    void notifyTrade(bt::Trade& trade) override {
        if (!trade.isOpen) {
            std::cout << "平仓盈亏: " << std::fixed << std::setprecision(2) 
                      << trade.pnlComm << std::endl;
        }
    }

private:
    int fastPeriod_;
    int slowPeriod_;
    
    double calculateSMA(const bt::LineBuffer& data, int period, int offset = 0) {
        if (data.length() < period + offset) return 0;
        double sum = 0;
        for (int i = offset; i < period + offset; ++i) {
            sum += data[i];
        }
        return sum / period;
    }
};

int main() {
    bt::Cerebro cerebro;
    
    // 加载数据
    auto data = std::make_shared<bt::BacktraderCSVData>("data.csv");
    data->load();
    cerebro.addData(data);
    
    // 配置经纪商
    cerebro.setCash(100000.0);
    cerebro.broker().setCommission(std::make_shared<bt::CommInfoStock>(0.001));
    
    // 添加策略
    cerebro.addStrategy<SMACrossover>(10, 30);
    
    // 添加分析器
    cerebro.addAnalyzer<bt::SharpeRatio>();
    cerebro.addAnalyzer<bt::DrawDown>();
    
    // 运行回测
    std::cout << "初始资金: " << cerebro.broker().getCash() << std::endl;
    auto results = cerebro.run();
    
    // 输出结果
    if (!results.empty()) {
        std::cout << "最终资产: " << results[0].endValue << std::endl;
        std::cout << "总收益率: " << results[0].pnlPct << "%" << std::endl;
        std::cout << "总交易次数: " << results[0].totalTrades << std::endl;
    }
    
    return 0;
}
```

### 10.2 RSI 均值回归策略

```cpp
class RSIMeanReversion : public bt::Strategy {
public:
    RSIMeanReversion(int rsiPeriod = 14, double oversold = 30, double overbought = 70)
        : rsiPeriod_(rsiPeriod), oversold_(oversold), overbought_(overbought) {}
    
    void init() override {
        setMinPeriod(rsiPeriod_ + 1);
    }
    
    void next() override {
        auto& close = data(0)->close();
        if (close.length() < rsiPeriod_ + 1) return;
        
        double rsi = calculateRSI(close, rsiPeriod_);
        bt::Value pos = position();
        
        // RSI 超卖买入
        if (rsi < oversold_ && pos == 0) {
            buy();
            std::cout << "RSI 超卖买入: RSI=" << rsi << std::endl;
        }
        // RSI 超买卖出
        else if (rsi > overbought_ && pos > 0) {
            closePosition();
            std::cout << "RSI 超买平仓: RSI=" << rsi << std::endl;
        }
    }

private:
    int rsiPeriod_;
    double oversold_;
    double overbought_;
    
    double calculateRSI(const bt::LineBuffer& data, int period) {
        double sumGain = 0, sumLoss = 0;
        
        for (int i = 0; i < period; ++i) {
            double change = data[i] - data[i + 1];
            if (change > 0) sumGain += change;
            else sumLoss -= change;
        }
        
        double avgGain = sumGain / period;
        double avgLoss = sumLoss / period;
        
        if (avgLoss == 0) return 100;
        double rs = avgGain / avgLoss;
        return 100 - 100 / (1 + rs);
    }
};
```

### 10.3 布林带突破策略

```cpp
class BollingerBreakout : public bt::Strategy {
private:
    bt::BollingerBands* bb_ = nullptr;
    
public:
    BollingerBreakout(int period = 20, double stdDev = 2.0)
        : period_(period), stdDev_(stdDev) {}
    
    void init() override {
        bb_ = new bt::BollingerBands(&data(0)->close(), period_, stdDev_);
        setMinPeriod(period_);
    }
    
    void next() override {
        double close = data(0)->close()[0];
        double upper = bb_->top()[0];
        double lower = bb_->bot()[0];
        bt::Value pos = position();
        
        // 突破上轨做多
        if (close > upper && pos <= 0) {
            if (pos < 0) closePosition();
            buy();
        }
        // 突破下轨做空
        else if (close < lower && pos >= 0) {
            if (pos > 0) closePosition();
            sell();
        }
        // 回归中轨平仓
        else if (pos != 0) {
            double mid = bb_->mid()[0];
            if ((pos > 0 && close < mid) || (pos < 0 && close > mid)) {
                closePosition();
            }
        }
    }
    
    ~BollingerBreakout() {
        delete bb_;
    }

private:
    int period_;
    double stdDev_;
};
```

---

## 11. Python 迁移指南

### 11.1 API 对照表

| Python Backtrader | C++ Backtrader |
|-------------------|----------------|
| `self.data.close[0]` | `data(0)->close()[0]` |
| `self.data.close[-1]` | `data(0)->close()[1]` ⚠️ 注意符号 |
| `self.datas[1].close[0]` | `data(1)->close()[0]` |
| `bt.indicators.SMA(period=20)` | `bt::SMA(&close, 20)` |
| `params = (('period', 14),)` | `BT_PARAM(period, 14)` |
| `self.p.period` | `p().get<int>("period")` |
| `self.buy()` | `buy()` |
| `self.sell()` | `sell()` |
| `self.close()` | `closePosition()` |
| `self.position.size` | `position()` |
| `self.broker.getvalue()` | `getBroker()->getValue()` |
| `self.broker.getcash()` | `getBroker()->getCash()` |
| `cerebro.addstrategy(MyStrategy)` | `cerebro.addStrategy<MyStrategy>()` |
| `cerebro.broker.setcash(100000)` | `cerebro.setCash(100000)` |
| `cerebro.broker.setcommission(commission=0.001)` | `cerebro.broker().setCommission(...)` |

### 11.2 迁移示例

**Python 版本：**
```python
class MyStrategy(bt.Strategy):
    params = (('period', 20),)
    
    def __init__(self):
        self.sma = bt.indicators.SMA(period=self.p.period)
    
    def next(self):
        if self.position.size == 0:
            if self.data.close[0] > self.sma[0]:
                self.buy()
        else:
            if self.data.close[0] < self.sma[0]:
                self.close()
```

**C++ 版本：**
```cpp
class MyStrategy : public bt::Strategy {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 20)
    BT_PARAMS_END()
    
    void init() override {
        sma_ = new bt::SMA(&data(0)->close(), p().get<int>("period"));
        setMinPeriod(p().get<int>("period"));
    }
    
    void next() override {
        if (position() == 0) {
            if (data(0)->close()[0] > sma_->value()) {
                buy();
            }
        } else {
            if (data(0)->close()[0] < sma_->value()) {
                closePosition();
            }
        }
    }
    
    ~MyStrategy() { delete sma_; }
    
private:
    bt::SMA* sma_ = nullptr;
};
```

---

## 12. 调试技巧

### 12.1 常见问题排查

**问题：指标值为 NaN**
```cpp
void next() override {
    bt::Value smaValue = sma_->value();
    
    // 检查 NaN
    if (bt::isnan(smaValue)) {
        std::cout << "警告: SMA 值为 NaN，可能数据不足" << std::endl;
        return;
    }
}
```

**问题：索引越界**
```cpp
void next() override {
    // 安全访问：检查数据长度
    if (data(0)->close().length() < 20) {
        return;
    }
    
    // 现在可以安全访问 [0] 到 [19]
    for (int i = 0; i < 20; ++i) {
        bt::Value price = data(0)->close()[i];
    }
}
```

**问题：订单未成交**
```cpp
void notifyOrder(Order& order) override {
    // 打印所有订单状态变化
    std::cout << "订单状态: " << static_cast<int>(order.status())
              << " 类型: " << static_cast<int>(order.type())
              << " 价格: " << order.price()
              << " 数量: " << order.size()
              << std::endl;
}
```

### 12.2 日志输出

```cpp
class DebugStrategy : public bt::Strategy {
public:
    void next() override {
        // 打印每个 bar 的信息
        auto dt = data(0)->getDateTime(0);
        std::cout << "[" << dt.toString() << "] "
                  << "O:" << data(0)->open()[0]
                  << " H:" << data(0)->high()[0]
                  << " L:" << data(0)->low()[0]
                  << " C:" << data(0)->close()[0]
                  << " V:" << data(0)->volume()[0]
                  << " Pos:" << position()
                  << std::endl;
    }
};
```

### 12.3 GTest 测试

```cpp
#include <gtest/gtest.h>
#include "bt/backtrader.hpp"

TEST(StrategyTest, BuySignal) {
    bt::Cerebro cerebro;
    
    auto data = std::make_shared<bt::MemoryDataFeed>();
    // 添加测试数据...
    cerebro.addData(data);
    
    cerebro.addStrategy<MyStrategy>();
    auto results = cerebro.run();
    
    EXPECT_GT(results[0].totalTrades, 0);
}
```

---

## 13. 性能优化

### 13.1 使用向量化计算

```cpp
class OptimizedIndicator : public bt::Indicator {
public:
    // 实现 once() 进行批量计算
    void once(Size start, Size end) override {
        // 使用 SIMD 优化
        const auto* rawData = singleLine_->rawData();
        std::vector<bt::Value> output(end - start);
        
        bt::simd::slidingMean(
            rawData->data() + start,
            output.data(),
            end - start,
            period_
        );
        
        for (const auto& v : output) {
            lines0().push(v);
        }
    }
};
```

### 13.2 减少内存分配

```cpp
class EfficientStrategy : public bt::Strategy {
private:
    // 预分配缓冲区
    std::vector<bt::Value> buffer_;
    
public:
    void init() override {
        buffer_.reserve(1000);  // 预分配
    }
};
```

### 13.3 使用 QBuffer 模式

```cpp
// 仅保留最近 100 个值，节省内存
bt::LineBuffer buffer(100);  // QBuffer 模式
```

### 13.4 多线程优化

```cpp
// 使用 Optimizer 的并行计算
bt::Optimizer optimizer(cerebro);
optimizer.setMaxWorkers(4);  // 4 线程并行
auto results = optimizer.run();
```

---

## 附录

### A. 类型定义

```cpp
namespace bt {
    using Size = std::size_t;      // 大小/长度
    using Index = std::ptrdiff_t;  // 支持负索引
    using Value = double;          // 数值类型
    constexpr Value NaN = ...;     // 无效值标记
}
```

### B. 订单类型

```cpp
enum class OrderType {
    Market,         // 市价单
    Close,          // 收盘价单
    Limit,          // 限价单
    Stop,           // 止损单
    StopLimit,      // 止损限价单
    StopTrail,      // 追踪止损
    StopTrailLimit, // 追踪止损限价
    Historical      // 历史订单
};
```

### C. 订单状态

```cpp
enum class OrderStatus {
    Created,     // 已创建
    Submitted,   // 已提交
    Accepted,    // 已接受
    Partial,     // 部分成交
    Completed,   // 全部成交
    Canceled,    // 已取消
    Expired,     // 已过期
    Margin,      // 保证金不足
    Rejected     // 被拒绝
};
```

### D. 常用头文件

| 头文件 | 用途 |
|--------|------|
| `bt/backtrader.hpp` | 主头文件（包含所有功能） |
| `bt/strategy.hpp` | 策略基类 |
| `bt/indicator.hpp` | 指标基类 |
| `bt/linebuffer.hpp` | 时间序列缓冲区 |
| `bt/cerebro.hpp` | 回测引擎 |
| `bt/broker.hpp` | 经纪商 |
| `bt/order.hpp` | 订单系统 |

---

> **文档版本**: 0.4.0  
> **最后更新**: 2026-01-08  
> **GitHub**: https://github.com/followtheart/backtrader.plus
