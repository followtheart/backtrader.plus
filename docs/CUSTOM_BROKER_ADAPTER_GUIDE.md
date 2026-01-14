# Backtrader C++ 自定义 Broker 适配开发指南

本文档旨在指导开发人员如何在 `backtrader.plus` (C++版) 中实现和适配自定义 Broker，以对接实盘交易接口（如 CTP, OKX, Binance 等）。

## 概述

`backtrader.plus` 采用了开放的架构设计，`bt::Broker` 类已被设计为虚基类，支持通过继承实现多态。这意味着开发者可以轻松地实现自己的 Broker 类，覆盖核心交易逻辑，并将其注入到 `Cerebro` 引擎中，从而实现从回测到实盘的无缝切换。

## 开发步骤

### 1. 继承 Broker 基类

首先，创建一个继承自 `bt::Broker` 的新类。由于基类中的核心方法（`buy`, `sell`, `next`, `getCash` 等）均已声明为 `virtual`，您可以直接重写它们。同时，基类的成员变量（如 `orders_`, `positions_`）通过 `protected` 权限开放，方便子类直接管理状态。

```cpp
#include "bt/broker/broker.hpp"

class MyExchangeBroker : public bt::Broker {
public:
    MyExchangeBroker() : bt::Broker() {
        // 初始化 API 连接，例如 websocket 客户端
    }

    virtual ~MyExchangeBroker() {
        // 清理资源
    }

    // ... 实现虚函数
};
```

### 2. 核心接口覆盖

#### A. 下单接口 (`buy` / `sell`)

您需要拦截 `buy` 和 `sell` 请求，先调用基类方法创建标准的内部 `Order` 对象，然后将其发送到交易所 API。

```cpp
Order* buy(const std::string& data, Size size, Value price, OrderType type) override {
    // 1. 调用基类逻辑创建本地 Order 对象 (保持引擎内部状态一致)
    // 这是必须的，因为 Strategy 持有的是这个 Order 对象的引用
    Order* order = bt::Broker::buy(data, size, price, type);
    
    // 2. 将订单转换为交易所 API 格式并发送
    // 注意：data 参数通常是数据源名称，需要映射为交易所 Symbol
    std::string symbol = mapDataToSymbol(data);
    
    // 假设 submitOrder 是您的 API 方法
    bool submitted = exchangeApi_.submitOrder(symbol, "buy", size, price);

    if (submitted) {
        // 如果 API 返回成功，标记状态为 Submitted
        order->setStatus(OrderStatus::Submitted);
    } else {
        // 如果发送失败，标记为 Rejected
        order->setStatus(OrderStatus::Rejected);
    }
    
    return order;
}

Order* sell(const std::string& data, Size size, Value price, OrderType type) override {
    Order* order = bt::Broker::sell(data, size, price, type);
    // ... 类似的对接逻辑
    return order;
}
```

#### B. 撤单接口 (`cancel`)

```cpp
void cancel(Size orderId) override {
    // 1. 在本地订单列表中查找订单
    // 由于 orders_ 是 protected 的，可以直接遍历
    for (const auto& orderPtr : orders_) {
        if (orderPtr->ref() == orderId) {
            // 2. 调用 API 撤单
            exchangeApi_.cancelOrder(orderId);
            break;
        }
    }
    // 3. 调用基类更新本地状态
    bt::Broker::cancel(orderId);
}
```

#### C. 状态同步 (`next`)

`next()` 方法在每个 Bar 结束时触发。在实盘模式下，这是同步账户资金、持仓和订单状态的最佳时机。

```cpp
void next() override {
    // 1. 同步账户资金
    // 直接修改 protected 成员 cash_ 和 startCash_
    this->cash_ = exchangeApi_.getAvailableBalance();
    
    // 2. 同步持仓
    auto positions = exchangeApi_.getPositions();
    for (auto& pos : positions) {
        // 更新 positions_ 映射表
        // PositionInfo { Value size; Value price; }
        this->positions_[pos.symbol].size = pos.amount;
        this->positions_[pos.symbol].price = pos.avgPrice;
    }

    // 3. 处理订单状态更新 (如果 API 是轮询模式)
    // 如果是 Websocket 推送模式，建议将消息推入队列，在此处消费队列更新 Order 状态
    updateOrderStatuses();

    // 注意：实盘模式下通常不需要调用 bt::Broker::next()
    // 因为父类的 next() 主要是用于回测时的模拟撮合逻辑
}
```

#### D. 资金查询接口

直接返回缓存的实盘数据。

```cpp
Value getCash() const override { return cash_; } // 或直接调用 API
Value getValue() const override { 
    // 返回总权益 (余额 + 未结盈亏)
    return exchangeApi_.getTotalEquity(); 
}
```

### 3. 集成到 Cerebro

使用 `cerebro.setBroker()` 注入您的自定义 Broker。

```cpp
int main() {
    bt::Cerebro cerebro;

    // 1. 创建并配置自定义 Broker
    auto myBroker = std::make_unique<MyExchangeBroker>();
    myBroker->setApiKey("API_KEY", "SECRET_KEY");
    
    // 2. 注入 Broker (所有权转移)
    // 注意：setBroker 接受 unique_ptr
    cerebro.setBroker(std::move(myBroker));

    // 3. 添加策略和数据
    auto data = std::make_shared<bt::BacktraderCSVData>("btc_usdt.csv");
    cerebro.addData(data, "BTC-USDT");
    cerebro.addStrategy<MyStrategy>();

    // 4. 启动
    cerebro.run();
}
```

## 关键技术点

### 1. 异步与同步的桥接
Backtrader 是基于事件循环（`next` 驱动）的同步框架，而现代交易 API（尤其是 WebSocket）是异步推送的。
*   **推荐模式**: 在 `MyExchangeBroker` 内部维护并发安全的队列（如 `ConcurrentQueue`）。WS 线程收到 `OnOrderUpdate` 或 `OnTrade` 时推入队列，主线程在 `next()` 中消费队列并在安全的主线程上下文中更新 `orders_` 状态。

### 2. 标的映射 (Symbol Mapping)
`Cerebro` 中添加数据可能是 `cerebro.addData(data, "Data0")`，而交易所需要 "BTC-USDT"。
*   建议在 Broker 中维护 `std::map<std::string, std::string> dataToSymbol_`，提供注册接口供初始化时配置。

### 3. 订单对象生命周期
`bt::Broker` 使用 `std::vector<std::unique_ptr<Order>> orders_` 管理所有历史订单。
*   自定义 Broker **必须**确保持有 `Broker::buy/sell` 创建的 `Order` 指针，切勿在本地重新 `new Order` 替代它，否则策略端持有的引用将失效。

## 回测与实盘的一致性

为了保证策略代码一套通用，策略中应始终使用标准 API：
*   `this->buy()` / `this->sell()`
*   `this->broker()->getCash()`
*   `this->position()`

不要在策略代码中直接调用 `MyExchangeBroker` 的特有方法。如果需要传入实盘参数（如滑点设置差异），应通过 `Broker::Params` 或配置文件在初始化阶段完成。

---
