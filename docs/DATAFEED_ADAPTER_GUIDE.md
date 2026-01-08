# Backtrader C++ 第三方数据源适配开发指南

本文档面向二次开发人员，详细介绍如何在 Backtrader C++ 框架中适配第三方数据源。

---

## 📚 目录

1. [数据源架构概述](#1-数据源架构概述)
2. [核心类层次结构](#2-核心类层次结构)
3. [适配第三方数据源的三种方式](#3-适配第三方数据源的三种方式)
4. [方式一：继承 DataFeed 基类](#4-方式一继承-datafeed-基类)
5. [方式二：继承 GenericCSVData 类](#5-方式二继承-genericcsvdata-类)
6. [方式三：使用 MemoryDataFeed](#6-方式三使用-memorydatafeed)
7. [高级适配：实时数据流](#7-高级适配实时数据流)
8. [实战案例](#8-实战案例)
9. [最佳实践与注意事项](#9-最佳实践与注意事项)
10. [API 参考](#10-api-参考)

---

## 1. 数据源架构概述

Backtrader C++ 的数据源系统基于 **LineSeries** 时间序列容器构建，采用分层设计：

```
LineBuffer (单条时间序列)
    ↓
LineSeries (多线容器)
    ↓
OHLCVData (标准金融数据：Open, High, Low, Close, Volume, OpenInterest)
    ↓
DataFeed (数据源基类，添加 datetime 线)
    ↓
GenericCSVData / MemoryDataFeed / 自定义数据源
```

### 核心数据结构

- **LineBuffer**: 存储单条时间序列数据，支持 `[0]` 当前值、`[1]` 前一值的索引方式
- **LineSeries**: 包含多个 LineBuffer 的容器
- **OHLCVData**: 预定义 6 条标准金融数据线
- **DataFeed**: 在 OHLCV 基础上添加 datetime 线

---

## 2. 核心类层次结构

```cpp
// 继承层次
class LineSeries : public Parametrized<LineSeries>
class OHLCVData : public LineSeries
class DataFeed : public OHLCVData
class GenericCSVData : public DataFeed    // CSV 解析器
class BacktraderCSVData : public GenericCSVData
class YahooFinanceData : public GenericCSVData
class MemoryDataFeed : public DataFeed    // 内存数据
```

### DataFeed 核心接口

```cpp
class DataFeed : public OHLCVData {
public:
    // 必须实现的纯虚函数
    virtual bool load() = 0;  // 加载数据
    
    // 可选重写
    virtual void preload() { load(); }  // 预加载
    
    // 数据访问
    LineBuffer& datetime();       // 日期时间线
    LineBuffer& open();           // 开盘价
    LineBuffer& high();           // 最高价
    LineBuffer& low();            // 最低价
    LineBuffer& close();          // 收盘价
    LineBuffer& volume();         // 成交量
    LineBuffer& openinterest();   // 持仓量
    
    // 辅助方法
    void setName(const std::string& name);
    Size length() const;
    bool next();    // 移动到下一个 bar
    void reset();   // 重置到起点
};
```

---

## 3. 适配第三方数据源的三种方式

| 方式 | 适用场景 | 复杂度 |
|------|----------|--------|
| 继承 DataFeed | 自定义文件格式、数据库、API | 高 |
| 继承 GenericCSVData | CSV 格式变体 | 低 |
| MemoryDataFeed | 已加载到内存的数据、程序生成的数据 | 最低 |

---

## 4. 方式一：继承 DataFeed 基类

适用于需要从数据库、网络 API 或非 CSV 文件格式加载数据的场景。

### 4.1 基本模板

```cpp
#include "bt/datafeed.hpp"

class MyCustomDataFeed : public bt::DataFeed {
public:
    // 可选：定义参数
    BT_PARAMS_BEGIN()
        BT_PARAM(source, "")        // 数据源标识
        BT_PARAM(start_date, "")    // 起始日期
        BT_PARAM(end_date, "")      // 结束日期
    BT_PARAMS_END()
    
    MyCustomDataFeed() = default;
    
    // 构造函数可接收配置参数
    explicit MyCustomDataFeed(const std::string& source) {
        p().set("source", source);
    }
    
    // 必须实现：加载数据
    bool load() override {
        // 1. 连接数据源
        // 2. 读取数据
        // 3. 调用 addBar() 和 datetime().push() 添加每条记录
        // 4. 返回是否成功
        return true;
    }
};
```

### 4.2 完整示例：从 JSON 文件加载

```cpp
#include "bt/datafeed.hpp"
#include <fstream>

// 假设使用 nlohmann/json 库（需自行集成）
// #include <nlohmann/json.hpp>

class JsonDataFeed : public bt::DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(filepath, "")
    BT_PARAMS_END()
    
    explicit JsonDataFeed(const std::string& path) {
        p().set("filepath", path);
    }
    
    bool load() override {
        std::string filepath = p().get<std::string>("filepath");
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        
        // 解析 JSON（伪代码）
        // nlohmann::json data = nlohmann::json::parse(file);
        // for (auto& bar : data["bars"]) {
        //     bt::DateTime dt = bt::DateTime::parse(bar["date"]);
        //     bt::Value o = bar["open"];
        //     bt::Value h = bar["high"];
        //     bt::Value l = bar["low"];
        //     bt::Value c = bar["close"];
        //     bt::Value v = bar["volume"];
        //     
        //     addBar(o, h, l, c, v, 0);
        //     datetime().push(dt.toDouble());
        // }
        
        return length() > 0;
    }
};
```

### 4.3 完整示例：从数据库加载

```cpp
#include "bt/datafeed.hpp"

class DatabaseDataFeed : public bt::DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(symbol, "")
        BT_PARAM(db_host, "localhost")
        BT_PARAM(db_port, 3306)
        BT_PARAM(db_name, "market_data")
    BT_PARAMS_END()
    
    explicit DatabaseDataFeed(const std::string& symbol) {
        p().set("symbol", symbol);
    }
    
    bool load() override {
        std::string symbol = p().get<std::string>("symbol");
        
        // 1. 建立数据库连接（伪代码）
        // auto conn = connectDB(host, port, db);
        
        // 2. 执行查询
        // auto result = conn.query(
        //     "SELECT date, open, high, low, close, volume "
        //     "FROM daily_bars WHERE symbol = ? ORDER BY date",
        //     symbol);
        
        // 3. 遍历结果集，添加数据
        // while (result.next()) {
        //     bt::DateTime dt = bt::DateTime::parse(result.getString("date"));
        //     addBar(
        //         result.getDouble("open"),
        //         result.getDouble("high"),
        //         result.getDouble("low"),
        //         result.getDouble("close"),
        //         result.getDouble("volume"),
        //         0  // openinterest
        //     );
        //     datetime().push(dt.toDouble());
        // }
        
        return length() > 0;
    }
};
```

---

## 5. 方式二：继承 GenericCSVData 类

适用于各种 CSV 格式的数据文件，只需配置列映射。

### 5.1 GenericCSVData 参数说明

```cpp
BT_PARAMS_BEGIN()
    BT_PARAM(datetime, 0)      // 日期时间列索引
    BT_PARAM(open, 1)          // 开盘价列索引
    BT_PARAM(high, 2)          // 最高价列索引
    BT_PARAM(low, 3)           // 最低价列索引
    BT_PARAM(close, 4)         // 收盘价列索引
    BT_PARAM(volume, 5)        // 成交量列索引
    BT_PARAM(openinterest, -1) // 持仓量列索引 (-1 = 不存在)
    BT_PARAM(dtformat, 0)      // 日期格式: 0=自动, 1=YYYY-MM-DD, 2=YYYY-MM-DD HH:MM:SS
    BT_PARAM(header, 1)        // 跳过的标题行数
    BT_PARAM(separator, 0)     // 分隔符: 0=逗号, 1=Tab, 2=分号
BT_PARAMS_END()
```

### 5.2 示例：适配通达信导出格式

```cpp
// 通达信导出格式（假设）：
// 日期,开盘,最高,最低,收盘,成交量,成交额
// 2024-01-02,100.00,102.50,99.50,101.00,1000000,100500000

class TongdaxinData : public bt::GenericCSVData {
public:
    TongdaxinData() {
        p().set("datetime", 0);     // 日期在第 0 列
        p().set("open", 1);
        p().set("high", 2);
        p().set("low", 3);
        p().set("close", 4);
        p().set("volume", 5);
        p().set("openinterest", -1); // 无持仓量
        p().set("header", 1);        // 1 行标题
        p().set("separator", 0);     // 逗号分隔
    }
    
    explicit TongdaxinData(const std::string& filepath)
        : TongdaxinData() {
        setFilepath(filepath);
    }
};
```

### 5.3 示例：适配 TuShare 格式

```cpp
// TuShare 格式：
// ts_code,trade_date,open,high,low,close,vol,amount
// 000001.SZ,20240102,100.00,102.50,99.50,101.00,1000000,100500000

class TuShareData : public bt::GenericCSVData {
public:
    TuShareData() {
        p().set("datetime", 1);     // trade_date 在第 1 列
        p().set("open", 2);
        p().set("high", 3);
        p().set("low", 4);
        p().set("close", 5);
        p().set("volume", 6);       // vol
        p().set("openinterest", -1);
        p().set("header", 1);
        p().set("separator", 0);
    }
    
    explicit TuShareData(const std::string& filepath)
        : TuShareData() {
        setFilepath(filepath);
    }
};
```

### 5.4 示例：适配 Wind 导出格式

```cpp
// Wind 导出格式（Tab 分隔）：
// 日期	开盘价	最高价	最低价	收盘价	成交量	持仓量

class WindData : public bt::GenericCSVData {
public:
    WindData() {
        p().set("datetime", 0);
        p().set("open", 1);
        p().set("high", 2);
        p().set("low", 3);
        p().set("close", 4);
        p().set("volume", 5);
        p().set("openinterest", 6);
        p().set("header", 1);
        p().set("separator", 1);     // Tab 分隔
    }
    
    explicit WindData(const std::string& filepath)
        : WindData() {
        setFilepath(filepath);
    }
};
```

---

## 6. 方式三：使用 MemoryDataFeed

适用于已加载到内存的数据或程序生成的测试数据。

### 6.1 基本用法

```cpp
#include "bt/datafeed.hpp"

int main() {
    // 创建内存数据源
    auto data = std::make_shared<bt::MemoryDataFeed>();
    
    // 逐条添加 Bar 数据
    data->addBar(
        bt::DateTime(2024, 1, 2),  // 日期时间
        100.0,   // open
        102.5,   // high
        99.5,    // low
        101.0,   // close
        1000000, // volume
        0        // openinterest (可选)
    );
    
    data->addBar(bt::DateTime(2024, 1, 3), 101.0, 103.0, 100.0, 102.5, 1200000);
    data->addBar(bt::DateTime(2024, 1, 4), 102.5, 104.0, 101.5, 103.5, 1100000);
    
    // 添加到 Cerebro
    bt::Cerebro cerebro;
    cerebro.addData(data, "SAMPLE");
    // ...
}
```

### 6.2 从外部数据结构批量导入

```cpp
// 假设从其他系统获取的数据结构
struct ExternalBar {
    std::string date;
    double o, h, l, c;
    long volume;
};

void loadFromExternalSource(bt::MemoryDataFeed& feed, 
                           const std::vector<ExternalBar>& bars) {
    for (const auto& bar : bars) {
        bt::DateTime dt = bt::DateTime::parse(bar.date);
        feed.addBar(dt, bar.o, bar.h, bar.l, bar.c, 
                   static_cast<bt::Value>(bar.volume));
    }
}
```

### 6.3 生成测试数据

```cpp
auto generateTestData(int numBars) {
    auto data = std::make_shared<bt::MemoryDataFeed>();
    
    double price = 100.0;
    std::srand(42);  // 固定随机种子便于复现
    
    for (int i = 0; i < numBars; ++i) {
        bt::DateTime dt(2024, 1 + i / 30, i % 30 + 1);
        
        // 随机价格变动
        double change = (std::rand() % 100 - 50) / 100.0;
        double o = price;
        double c = price + change;
        double h = std::max(o, c) + (std::rand() % 50) / 100.0;
        double l = std::min(o, c) - (std::rand() % 50) / 100.0;
        double v = 10000 + std::rand() % 5000;
        
        data->addBar(dt, o, h, l, c, v);
        price = c;
    }
    
    return data;
}
```

---

## 7. 高级适配：实时数据流

### 7.1 设计模式

对于实时数据源（如行情 API），建议使用**生产者-消费者模式**：

```cpp
#include "bt/datafeed.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>

class RealtimeDataFeed : public bt::DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(symbol, "")
    BT_PARAMS_END()
    
    explicit RealtimeDataFeed(const std::string& symbol) {
        p().set("symbol", symbol);
    }
    
    bool load() override {
        // 实时数据源启动时不加载历史数据
        // 而是等待实时数据推送
        return true;
    }
    
    // 由外部数据线程调用，推送新的 Bar 数据
    void pushBar(const bt::DateTime& dt, bt::Value o, bt::Value h, 
                 bt::Value l, bt::Value c, bt::Value v) {
        std::lock_guard<std::mutex> lock(mutex_);
        addBar(o, h, l, c, v, 0);
        datetime().push(dt.toDouble());
        hasNewData_ = true;
        cv_.notify_one();
    }
    
    // 等待新数据
    bool waitForData(int timeoutMs = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                           [this] { return hasNewData_; });
    }
    
    void clearNewDataFlag() {
        std::lock_guard<std::mutex> lock(mutex_);
        hasNewData_ = false;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool hasNewData_ = false;
};
```

### 7.2 与 Websocket 集成示例（伪代码）

```cpp
#include "bt/datafeed.hpp"

class WebsocketDataFeed : public bt::DataFeed {
public:
    explicit WebsocketDataFeed(const std::string& wsUrl) 
        : wsUrl_(wsUrl) {}
    
    bool load() override {
        // 1. 建立 Websocket 连接
        // ws_ = websocket::connect(wsUrl_);
        
        // 2. 注册消息回调
        // ws_.onMessage([this](const std::string& msg) {
        //     auto bar = parseTickData(msg);
        //     addBar(bar.o, bar.h, bar.l, bar.c, bar.v, 0);
        //     datetime().push(bar.dt.toDouble());
        // });
        
        return true;
    }
    
    void disconnect() {
        // ws_.close();
    }

private:
    std::string wsUrl_;
    // websocket::Client ws_;
};
```

---

## 8. 实战案例

### 8.1 完整案例：适配币安 API 数据

```cpp
#include "bt/datafeed.hpp"
#include <sstream>

// 模拟从币安 API 获取 K 线数据
class BinanceDataFeed : public bt::DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(symbol, "BTCUSDT")
        BT_PARAM(interval, "1d")
        BT_PARAM(limit, 500)
    BT_PARAMS_END()
    
    explicit BinanceDataFeed(const std::string& symbol, 
                            const std::string& interval = "1d") {
        p().set("symbol", symbol);
        p().set("interval", interval);
    }
    
    bool load() override {
        std::string symbol = p().get<std::string>("symbol");
        std::string interval = p().get<std::string>("interval");
        int limit = p().get<int>("limit");
        
        // 构建 API URL
        std::ostringstream url;
        url << "https://api.binance.com/api/v3/klines?"
            << "symbol=" << symbol
            << "&interval=" << interval
            << "&limit=" << limit;
        
        // 调用 HTTP 请求（需要集成 HTTP 库如 cpr、curl 等）
        // std::string response = httpGet(url.str());
        
        // 解析 JSON 响应
        // 币安返回格式：[[时间戳, 开, 高, 低, 收, 量, ...], ...]
        // for (auto& kline : json::parse(response)) {
        //     long timestamp = kline[0];
        //     bt::DateTime dt = timestampToDateTime(timestamp);
        //     addBar(
        //         std::stod(kline[1].get<std::string>()),  // open
        //         std::stod(kline[2].get<std::string>()),  // high
        //         std::stod(kline[3].get<std::string>()),  // low
        //         std::stod(kline[4].get<std::string>()),  // close
        //         std::stod(kline[5].get<std::string>()),  // volume
        //         0
        //     );
        //     datetime().push(dt.toDouble());
        // }
        
        return length() > 0;
    }
    
private:
    static bt::DateTime timestampToDateTime(long ts) {
        time_t t = ts / 1000;  // 币安时间戳是毫秒
        std::tm* tm = std::localtime(&t);
        return bt::DateTime(
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec
        );
    }
};
```

### 8.2 完整案例：适配欧易（OKX）API 数据

```cpp
#include "bt/datafeed.hpp"
#include <sstream>

/**
 * @brief 欧易（OKX）交易所 K 线数据源
 * 
 * OKX API 文档: https://www.okx.com/docs-v5/zh/
 * 
 * K 线接口: GET /api/v5/market/candles
 * 返回格式: [[ts, o, h, l, c, vol, volCcy, volCcyQuote, confirm], ...]
 * - ts: 开始时间戳（毫秒）
 * - o: 开盘价
 * - h: 最高价
 * - l: 最低价
 * - c: 收盘价
 * - vol: 交易量（币）
 * - volCcy: 交易量（计价货币）
 * - volCcyQuote: 交易量（报价货币）
 * - confirm: K 线状态 (0: 未完结, 1: 已完结)
 */
class OKXDataFeed : public bt::DataFeed {
public:
    BT_PARAMS_BEGIN()
        BT_PARAM(instId, "BTC-USDT")     // 产品ID，如 BTC-USDT, ETH-USDT-SWAP
        BT_PARAM(bar, "1D")              // K 线周期：1m/3m/5m/15m/30m/1H/2H/4H/6H/12H/1D/1W/1M
        BT_PARAM(limit, 300)             // 请求数量，最大 300
        BT_PARAM(instType, "SPOT")       // 产品类型：SPOT(币币), SWAP(永续), FUTURES(交割), OPTION(期权)
        BT_PARAM(after, "")              // 请求此时间戳之前的数据（分页用）
        BT_PARAM(before, "")             // 请求此时间戳之后的数据（分页用）
    BT_PARAMS_END()
    
    OKXDataFeed() = default;
    
    /**
     * @brief 构造函数
     * @param instId 产品ID，如 "BTC-USDT"
     * @param bar K 线周期，如 "1D", "1H", "15m"
     */
    explicit OKXDataFeed(const std::string& instId, 
                        const std::string& bar = "1D") {
        p().set("instId", instId);
        p().set("bar", bar);
    }
    
    /**
     * @brief 设置为永续合约模式
     */
    void setSwapMode() {
        std::string instId = p().get<std::string>("instId");
        // 如果不包含 -SWAP 后缀，自动添加
        if (instId.find("-SWAP") == std::string::npos) {
            p().set("instId", instId + "-SWAP");
        }
        p().set("instType", "SWAP");
    }
    
    /**
     * @brief 设置为交割合约模式
     * @param expiry 到期日，如 "240329" 表示 2024年3月29日
     */
    void setFuturesMode(const std::string& expiry) {
        std::string instId = p().get<std::string>("instId");
        // 构建交割合约 ID，如 BTC-USDT-240329
        size_t pos = instId.find("-SWAP");
        if (pos != std::string::npos) {
            instId = instId.substr(0, pos);
        }
        p().set("instId", instId + "-" + expiry);
        p().set("instType", "FUTURES");
    }
    
    bool load() override {
        std::string instId = p().get<std::string>("instId");
        std::string bar = p().get<std::string>("bar");
        int limit = p().get<int>("limit");
        std::string after = p().get<std::string>("after");
        std::string before = p().get<std::string>("before");
        
        // 构建 API URL
        // OKX API 基础地址：https://www.okx.com
        // 备用地址（AWS）：https://aws.okx.com
        std::ostringstream url;
        url << "https://www.okx.com/api/v5/market/candles?"
            << "instId=" << instId
            << "&bar=" << bar
            << "&limit=" << limit;
        
        // 可选分页参数
        if (!after.empty()) {
            url << "&after=" << after;
        }
        if (!before.empty()) {
            url << "&before=" << before;
        }
        
        // ============================================
        // 调用 HTTP 请求（需要集成 HTTP 库）
        // 推荐使用: cpr, libcurl, boost::beast 等
        // ============================================
        // std::string response = httpGet(url.str());
        
        // ============================================
        // 解析 JSON 响应
        // OKX 响应格式:
        // {
        //   "code": "0",
        //   "msg": "",
        //   "data": [
        //     ["1704067200000", "42000.5", "42500.0", "41800.0", "42300.0", 
        //      "1234.56", "51234567.89", "51234567.89", "1"],
        //     ...
        //   ]
        // }
        // ============================================
        
        // 示例解析代码（伪代码）：
        /*
        auto json = nlohmann::json::parse(response);
        
        // 检查响应状态
        if (json["code"].get<std::string>() != "0") {
            // 错误处理
            std::cerr << "OKX API Error: " << json["msg"].get<std::string>() << std::endl;
            return false;
        }
        
        auto& data = json["data"];
        
        // OKX 返回数据是倒序的（最新在前），需要反转
        std::vector<std::tuple<bt::DateTime, bt::Value, bt::Value, bt::Value, bt::Value, bt::Value>> bars;
        
        for (auto& candle : data) {
            long timestamp = std::stol(candle[0].get<std::string>());
            bt::DateTime dt = timestampToDateTime(timestamp);
            
            bt::Value o = std::stod(candle[1].get<std::string>());
            bt::Value h = std::stod(candle[2].get<std::string>());
            bt::Value l = std::stod(candle[3].get<std::string>());
            bt::Value c = std::stod(candle[4].get<std::string>());
            bt::Value v = std::stod(candle[5].get<std::string>());  // 交易量（币）
            
            // 可选：过滤未完结的 K 线
            std::string confirm = candle[8].get<std::string>();
            if (confirm == "0") {
                continue;  // 跳过未完结 K 线
            }
            
            bars.emplace_back(dt, o, h, l, c, v);
        }
        
        // 反转为正序（旧数据在前）
        std::reverse(bars.begin(), bars.end());
        
        // 添加到数据源
        for (const auto& [dt, o, h, l, c, v] : bars) {
            addBar(o, h, l, c, v, 0);
            datetime().push(dt.toDouble());
        }
        */
        
        return length() > 0;
    }
    
    /**
     * @brief 加载历史数据（支持分页获取更多数据）
     * @param totalBars 需要的总 K 线数量
     * @return 是否成功
     */
    bool loadHistory(int totalBars) {
        int loaded = 0;
        std::string lastTimestamp;
        
        while (loaded < totalBars) {
            if (!lastTimestamp.empty()) {
                p().set("after", lastTimestamp);
            }
            
            Size beforeLoad = length();
            if (!load()) {
                break;
            }
            
            Size newBars = length() - beforeLoad;
            if (newBars == 0) {
                break;  // 没有更多数据
            }
            
            loaded += static_cast<int>(newBars);
            
            // 获取最早的时间戳用于下次请求
            // lastTimestamp = getEarliestTimestamp();
        }
        
        return length() > 0;
    }

private:
    static bt::DateTime timestampToDateTime(long ts) {
        time_t t = ts / 1000;  // OKX 时间戳是毫秒
        std::tm* tm = std::localtime(&t);
        return bt::DateTime(
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec
        );
    }
};

/**
 * @brief OKX 数据源使用示例
 */
void okxDataFeedExample() {
    bt::Cerebro cerebro;
    
    // 示例 1：现货数据
    auto spotData = std::make_shared<OKXDataFeed>("BTC-USDT", "1D");
    spotData->load();
    cerebro.addData(spotData, "BTC-SPOT");
    
    // 示例 2：永续合约数据
    auto swapData = std::make_shared<OKXDataFeed>("ETH-USDT");
    swapData->setSwapMode();  // 设置为永续合约
    swapData->p().set("bar", "4H");  // 4 小时 K 线
    swapData->load();
    cerebro.addData(swapData, "ETH-SWAP");
    
    // 示例 3：交割合约数据
    auto futuresData = std::make_shared<OKXDataFeed>("BTC-USDT");
    futuresData->setFuturesMode("240329");  // 2024年3月29日到期
    futuresData->load();
    cerebro.addData(futuresData, "BTC-FUTURES");
    
    // 配置并运行回测...
}
```

#### OKX K 线周期参数对照表

| 周期代码 | 说明 | 周期代码 | 说明 |
|----------|------|----------|------|
| `1m` | 1 分钟 | `1H` | 1 小时 |
| `3m` | 3 分钟 | `2H` | 2 小时 |
| `5m` | 5 分钟 | `4H` | 4 小时 |
| `15m` | 15 分钟 | `6H` | 6 小时 |
| `30m` | 30 分钟 | `12H` | 12 小时 |
| `1D` | 1 天 | `1W` | 1 周 |
| `1M` | 1 月 | `3M` | 3 月 |

#### OKX 产品类型说明

| instType | 产品类型 | instId 示例 |
|----------|----------|-------------|
| `SPOT` | 币币交易 | BTC-USDT |
| `SWAP` | 永续合约 | BTC-USDT-SWAP |
| `FUTURES` | 交割合约 | BTC-USDT-240329 |
| `OPTION` | 期权 | BTC-USD-240329-50000-C |

### 8.3 完整案例：使用自定义数据源进行回测

```cpp
#include "bt/backtrader.hpp"
#include <iostream>

// 自定义数据源
class MyCSVData : public bt::GenericCSVData {
public:
    MyCSVData() {
        // 配置列映射
        p().set("datetime", 0);
        p().set("open", 1);
        p().set("high", 2);
        p().set("low", 3);
        p().set("close", 4);
        p().set("volume", 5);
        p().set("openinterest", -1);
        p().set("header", 1);
    }
    
    explicit MyCSVData(const std::string& path) : MyCSVData() {
        setFilepath(path);
    }
};

// 简单策略
class SimpleStrategy : public bt::Strategy {
public:
    void init() override {
        setMinPeriod(1);
    }
    
    void next() override {
        if (!data(0)) return;
        
        auto& close = data(0)->close();
        if (close.length() < 2) return;
        
        // 简单策略：价格上涨买入，下跌卖出
        if (position() == 0 && close[0] > close[1]) {
            buy();
        } else if (position() > 0 && close[0] < close[1]) {
            closePosition();
        }
    }
};

int main() {
    bt::Cerebro cerebro;
    
    // 使用自定义数据源
    auto data = std::make_shared<MyCSVData>("./data/my_stock.csv");
    if (!data->load()) {
        std::cerr << "Failed to load data" << std::endl;
        return 1;
    }
    
    cerebro.addData(data, "STOCK");
    cerebro.setCash(100000);
    cerebro.addStrategy<SimpleStrategy>();
    
    std::cout << "Starting Portfolio Value: " << cerebro.broker().getCash() << std::endl;
    
    auto results = cerebro.run();
    
    if (!results.empty()) {
        std::cout << "Final Portfolio Value: " << results[0].endValue << std::endl;
        std::cout << "Total Return: " << results[0].pnlPct << "%" << std::endl;
    }
    
    return 0;
}
```

---

## 9. 最佳实践与注意事项

### 9.1 数据验证

```cpp
bool load() override {
    // ... 加载数据 ...
    
    // 验证数据完整性
    if (length() == 0) {
        return false;
    }
    
    // 检查数据一致性
    for (bt::Size i = 0; i < length(); ++i) {
        bt::Value o = open()[i];
        bt::Value h = high()[i];
        bt::Value l = low()[i];
        bt::Value c = close()[i];
        
        // 基本校验
        if (bt::isnan(o) || bt::isnan(h) || bt::isnan(l) || bt::isnan(c)) {
            // 处理无效数据
            continue;
        }
        
        // OHLC 逻辑校验
        if (h < l || h < o || h < c || l > o || l > c) {
            // 数据异常警告
        }
    }
    
    return true;
}
```

### 9.2 错误处理

```cpp
bool load() override {
    try {
        // 尝试加载数据
        // ...
    } catch (const std::exception& e) {
        // 记录错误日志
        // log("Data load error: " + std::string(e.what()));
        return false;
    }
    
    return length() > 0;
}
```

### 9.3 性能优化

```cpp
bool load() override {
    // 预估数据量，提前分配内存
    Size estimatedBars = 1000;
    open().reserve(estimatedBars);
    high().reserve(estimatedBars);
    low().reserve(estimatedBars);
    close().reserve(estimatedBars);
    volume().reserve(estimatedBars);
    datetime().reserve(estimatedBars);
    
    // 加载数据...
    
    return true;
}
```

### 9.4 日期格式处理

```cpp
// DateTime::parse 支持的格式
// 格式 0 (自动): 自动检测
// 格式 1: YYYY-MM-DD
// 格式 2: YYYY-MM-DD HH:MM:SS

// 对于非标准格式，需要自定义解析
bt::DateTime parseCustomDate(const std::string& str) {
    // 例如：20240102 格式
    if (str.length() == 8) {
        return bt::DateTime(
            std::stoi(str.substr(0, 4)),   // year
            std::stoi(str.substr(4, 2)),   // month
            std::stoi(str.substr(6, 2))    // day
        );
    }
    return bt::DateTime();
}
```

### 9.5 多数据源支持

```cpp
int main() {
    bt::Cerebro cerebro;
    
    // 添加多个数据源
    auto stock1 = std::make_shared<MyCSVData>("stock1.csv");
    auto stock2 = std::make_shared<MyCSVData>("stock2.csv");
    auto stock3 = std::make_shared<MyCSVData>("stock3.csv");
    
    stock1->load();
    stock2->load();
    stock3->load();
    
    cerebro.addData(stock1, "AAPL");
    cerebro.addData(stock2, "GOOGL");
    cerebro.addData(stock3, "MSFT");
    
    // 在策略中通过 data(0), data(1), data(2) 访问
    // ...
}
```

---

## 10. API 参考

### DateTime 结构体

```cpp
struct DateTime {
    int year, month, day, hour, minute, second;
    
    DateTime();
    DateTime(int y, int m, int d, int h = 0, int min = 0, int s = 0);
    
    double toDouble() const;              // 转为 epoch 天数
    static DateTime parse(const std::string& str, int dtformat = 0);
    std::string toString() const;
};
```

### DataFeed 类

```cpp
class DataFeed : public OHLCVData {
public:
    // 必须实现
    virtual bool load() = 0;
    
    // 数据线访问
    LineBuffer& datetime();
    LineBuffer& open();
    LineBuffer& high();
    LineBuffer& low();
    LineBuffer& close();
    LineBuffer& volume();
    LineBuffer& openinterest();
    
    // 属性
    const std::string& name() const;
    void setName(const std::string& name);
    Size length() const;
    
    // 导航
    bool next();     // 移到下一 bar
    void reset();    // 重置到起点
};
```

### OHLCVData 类

```cpp
class OHLCVData : public LineSeries {
public:
    // 添加 Bar 数据
    void addBar(Value o, Value h, Value l, Value c, 
                Value v = 0.0, Value oi = 0.0);
    
    // 线索引常量
    static constexpr Size OPEN = 0;
    static constexpr Size HIGH = 1;
    static constexpr Size LOW = 2;
    static constexpr Size CLOSE = 3;
    static constexpr Size VOLUME = 4;
    static constexpr Size OPENINTEREST = 5;
};
```

### GenericCSVData 类

```cpp
class GenericCSVData : public DataFeed {
public:
    // 参数（通过 p().set/get 访问）
    // datetime, open, high, low, close, volume, openinterest: 列索引
    // dtformat: 日期格式
    // header: 跳过行数
    // separator: 分隔符类型
    
    void setFilepath(const std::string& filepath);
    const std::string& filepath() const;
    bool load() override;
};
```

### MemoryDataFeed 类

```cpp
class MemoryDataFeed : public DataFeed {
public:
    bool load() override;  // 直接返回 true
    
    void addBar(const DateTime& dt, Value o, Value h, Value l, Value c,
                Value v = 0.0, Value oi = 0.0);
};
```

---

## 附录：常见数据源格式参考

### CSV 格式数据源

| 数据源 | datetime | open | high | low | close | volume | separator | header |
|--------|----------|------|------|-----|-------|--------|-----------|--------|
| Yahoo Finance | 0 | 1 | 2 | 3 | 4 | 6 | , | 1 |
| TuShare | 1 | 2 | 3 | 4 | 5 | 6 | , | 1 |
| 通达信导出 | 0 | 1 | 2 | 3 | 4 | 5 | , | 1 |
| Wind | 0 | 1 | 2 | 3 | 4 | 5 | Tab | 1 |
| Quandl | 0 | 1 | 2 | 3 | 4 | 5 | , | 1 |

### 交易所 API 数据源

| 交易所 | API 返回格式 | 数据索引 (ts/o/h/l/c/v) | 时间戳格式 | 备注 |
|--------|--------------|-------------------------|------------|------|
| 币安 (Binance) | 数组 | 0/1/2/3/4/5 | 毫秒 | 数据正序 |
| 欧易 (OKX) | 数组 | 0/1/2/3/4/5 | 毫秒 | 数据倒序需反转 |
| 火币 (Huobi) | 对象数组 | id/open/high/low/close/vol | 秒 | - |
| Coinbase | 数组 | 0/3/2/1/4/5 | 秒 | 顺序不同 |
| Kraken | 数组 | 0/1/2/3/4/6 | 秒 | - |
| FTX (已关闭) | 对象数组 | startTime/open/high/low/close/volume | ISO 8601 | - |
| Bybit | 对象数组 | openTime/open/high/low/close/volume | 毫秒 | - |
| Gate.io | 数组 | 0/5/3/4/2/1 | 秒 | 顺序特殊 |
| Bitget | 数组 | 0/1/2/3/4/5 | 毫秒 | - |
| MEXC | 数组 | 0/1/2/3/4/5 | 毫秒 | 类似币安 |

---

**文档版本**: 1.1  
**适用版本**: Backtrader C++ 0.4.0+  
**最后更新**: 2026-01-08
